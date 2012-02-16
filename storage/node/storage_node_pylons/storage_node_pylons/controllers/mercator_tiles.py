from storage_node_pylons.lib.base import BaseController
from webob import exc
from pylons import request
from pylons import config
from pylons import response
from pylons.controllers.util import Response

import storage_node_pylons.lib.mqTile
import storage_node_pylons.lib.mqCache
import storage_node_pylons.lib.mqStats

import time
from calendar import timegm
import datetime
import logging
import os
import sys
import traceback

from urllib2 import URLError
from webob import exc

LAST_MODIFIED				= "last-modified"
REPLICA					= "X-Replica"
X_ALSO_EXPIRE                           = "X-Also-Expire"
DATE_FORMAT				= "%a, %d %b %Y %H:%M:%S %Z"
REQUEST_POST				= "POST"
INVALID_INPUT_ERROR_CODE		= 403
NODATA_ERROR_CODE 			= 404
INVALID_LATITUDE_ERROR_CODE		= 405
TIMEOUT_ERROR 				= 408
DISK_ERROR				= 502
SEVERE_ERROR 				= 503
SOURCE_ERROR 				= 504

DATETIME_EPOCH = datetime.datetime(1970, 1, 1)
EPOCH = timegm(DATETIME_EPOCH.timetuple())

log = logging.getLogger(__name__)
time_log = logging.getLogger("time")

class MercatorTilesController(BaseController):
	
	"""
	/tiles/1.0.0/{version:(vx|vy)}/{tile_type:[a-z]}/{zoom:[0-9]+}/{tx:[0-9]+}/{ty:[0-9]+}.{extension:(jpg|gif|pngi|json)}
	"""
	def version_handler( self, version, tile_type, zoom, tx, ty, extension ):
		return self.serve_tile( version, tile_type, zoom, tx, ty, extension )
			
	def formatExceptionInfo( self ):
		error_type, error_value, trbk = sys.exc_info()
		tb_list = traceback.format_tb(trbk)    
		s = "Error: %s \nDescription: %s \nTraceback:" % (error_type.__name__, error_value)
		for i in tb_list:
			s += "\n" + i
		return s
		
	def get_image_content_type(self, format):
		'''
		Convert an image format to a mime content type
		>>> get_image_content_type('gif')
		'image/gif'
		>>> get_image_content_type('png')
		'image/png'
		>>> get_image_content_type('jpg')
		'image/jpeg'
		>>> get_image_content_type('jpeg')
		'image/jpeg'
		>>> get_image_content_type('unk')
		'application/octet-stream'
		'''
		format = format.lower()
		if format == 'gif':
			return 'image/gif'
		elif format == 'png':
			return 'image/png'
		elif format == 'jpg' or format == 'jpeg':
			return 'image/jpeg'
		elif format == 'json':
			return 'application/json;charset=UTF-8'
		else:
			return 'application/octet-stream'
	
	def getDiskCache( self, tileInfo ):
		objCache	= None
		
		try:
			cacheRoot	= os.path.join( config['pylons.app_globals'].cacheInfo[ tileInfo.version ][ 'root' ], tileInfo.type )
			log.debug( "cache root " + cacheRoot )
			
			objCache	= storage_node_pylons.lib.mqCache.mqCache( cacheRoot )
			
		except:
			log.error( "Cache root construction failed for tile " + str( tileInfo ) )
			pass
			
		return objCache

	def getStats(self):
		return config['pylons.app_globals'].statsDB()

	def getExpiry(self):
		return config['pylons.app_globals'].expiryDB()
		
	def lastModifiedTime(self, objCache, theTile):
		# if this call returns None, indicating failure, then the
		# fall-back to the actual file time is still appropriate.
		if self.getExpiry().get_tile(theTile.x, theTile.y, theTile.zoom, theTile.type):
			return EPOCH
		else:
			return objCache.get_modified_time( theTile )

	def serve_tile( self, version, tile_type, zoom, tx, ty, extension):
		try:
			zoom = int(zoom)
			tx = int(tx)
			ty = int(ty)
		except:
			log.error('Incorrect request parameters')
			response.status_int = INVALID_INPUT_ERROR_CODE
			return      
		
		try:
			log.info(repr(request))
			
			raster_image, lastModified  = self.process_tile_request( version, tile_type, zoom, tx, ty, extension )

			resp = Response(body=raster_image, content_type=self.get_image_content_type(extension))
			
			if lastModified is not None:
				resp.last_modified = 1.0 * lastModified
	
			return resp 
 		
		except exc.HTTPForbidden, exp:
			error = self.formatExceptionInfo()
			log.error ("Tile request failed for a invalid values : %s; error trace %s " % (str(request), error))
			response.status_int = INVALID_INPUT_ERROR_CODE
			return      
		except URLError, exp:
			error = self.formatExceptionInfo()
			if(str(exp) == '<urlopen error timed out>'): #urllib2 does not have a separate error code for timeout.had to do string compare of error message
				log.error ("Timeout error for %s; error trace %s " % (str(request), error))
				response.status_int = TIMEOUT_ERROR
			else:
				#log.error ("Network error for %s; error trace %s " % (str(request), error))
				response.status_int = NODATA_ERROR_CODE
			return    
		except IOError, OSError:
			error = self.formatExceptionInfo()
			log.error ("Disk read error for url %s; error trace %s " % (str(request), error))
			response.status_int = DISK_ERROR
			return		
		except:
			error = self.formatExceptionInfo()
			log.fatal ("SEVERE error occurred url %s; error trace %s " % (str(request), error))
			response.status_int = SEVERE_ERROR
			return
 		
	def process_tile_request( self, version, tile_type, zoom, tx, ty, extension ):

		#try to get replication parameter out
		try:
			replicate = request.headers[REPLICA]
		except:
			replicate = '0'
		theTile = storage_node_pylons.lib.mqTile.mqTile( tile_type, zoom, tx, ty, extension, version, replicate)
		raster_image = None
		objCache = self.getDiskCache( theTile )
	
		if not objCache:
			raise IOError( "Map cache not configured for " + str( theTile ) )
    		#if its a post
		if request.method == REQUEST_POST:
			log.debug( "Post method found in process tile; attempting to store tile(s)" )
			raster_image = None
			for aKey in request.params.keys():
				if aKey.find( "file" ) < 0:
					continue

				start_time = datetime.datetime.now()

				#parse the file name for this file out of this part of the multi part post
				fileStorage = request.params[ aKey ]
				urlComps	= fileStorage.filename.encode('ascii', 'ignore').split( "/" )
				y		= int( urlComps[ len( urlComps ) - 1 ].split( "." )[0] )
				x		= int( urlComps[ len( urlComps ) - 2 ] )
				z		= int( urlComps[ len( urlComps ) - 3 ] )
				image_type 	= urlComps[ len( urlComps ) - 1 ].split( "." )[1]
				tile		= storage_node_pylons.lib.mqTile.mqTile( theTile.type, z, x, y, image_type, theTile.version, replicate )

				#store the file
				objCache.put( tile, fileStorage.value )

				end_time = datetime.datetime.now()
				self.getStats().update_post(self.usec_diff(end_time, start_time))

				#if they tried to send a modified time use it, otherwise use now
				try:
					newDateTime = datetime.datetime.strptime( request.headers[ LAST_MODIFIED ], DATE_FORMAT ) # Sun, 06 Nov 1994 08:49:37 GMT
				except:
					newDateTime = datetime.datetime.utcnow()
					pass #log.error( "exception attmpting to parse %s" % ( request.headers[ LAST_MODIFIED ] ) )

				#set the modified time
				log.debug( "Adding/updating tile "  + str( theTile ) + " " + str( newDateTime ) )

				secondsFromEpoch = timegm(newDateTime.timetuple())
				objCache.set_modified_time( tile, secondsFromEpoch)
				# failure here, while annoying, isn't a serious error as
				# the tile still has its time set above. we output a 
				# warning, as there may be other tiles in this metatile
				# not expired.
				expired_ok = self.getExpiry().set_tile(tile.x, tile.y, tile.zoom, tile.type, secondsFromEpoch == EPOCH)
				if not expired_ok:
					log.warning("Setting expiry information for %s failed." % str(theTile))

		   	#return the image and the time
			return raster_image, self.lastModifiedTime(objCache, theTile)
		#assume anything else is a get (probably should reject other requests DELETE,HEAD etc)
		else:
			start_time = datetime.datetime.now()

			#get the tile
			raster_image = objCache.get( theTile )
			if raster_image:
				#see if we need to set the modify time (dirty the file)
				try:
					newDateTime = datetime.datetime.strptime( request.headers[ LAST_MODIFIED ], DATE_FORMAT ) # Sun, 06 Nov 1994 08:49:37 GMT
					secondsFromEpoch = timegm(newDateTime.timetuple())
					expire_styles = [ theTile.type ]
					if X_ALSO_EXPIRE in request.headers:
						expire_styles = map(lambda s: s.strip(), request.headers[X_ALSO_EXPIRE].split(','))
					for style in expire_styles:
						alsoTile = storage_node_pylons.lib.mqTile.mqTile(style, theTile.zoom, theTile.x, theTile.y, theTile.extension, theTile.version, theTile.replica)
						objCache.set_modified_time( alsoTile, secondsFromEpoch)
						expired_ok = self.getExpiry().set_tile(alsoTile.x, alsoTile.y, alsoTile.zoom, style, secondsFromEpoch == EPOCH)
						if not expired_ok:
							log.warning("Setting expiry information for %s failed." % str(alsoTile))
				except Exception as e:
					log.debug("=== Got an exception: %s" % str(e))
			else:
				end_time = datetime.datetime.now()
				self.getStats().update_get(self.usec_diff(end_time, start_time))
				raise URLError("Tile not found in cache for %s "  % (str(theTile.type)))

			end_time = datetime.datetime.now()
			self.getStats().update_get(self.usec_diff(end_time, start_time))

			#return the image and the modified time
			return raster_image, self.lastModifiedTime(objCache, theTile)
    
	def usec_diff(self, t2, t1):
		td = t2 - t1
		return td.microseconds + (td.seconds + td.days * 24 * 3600) * 1000000
