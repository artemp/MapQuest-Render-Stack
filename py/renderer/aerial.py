#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Aerial and Sat Image Renderer
#
#  Author: john.novak@mapquest.com
#  Author: matt.amos@mapquest.com
#  Author: kevin.kreiser@mapquest.com
#
#  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#------------------------------------------------------------------------------

from mapnik2 import Map
from mapnik2 import load_map
from mapnik2 import Image
from mapnik2 import render
from mapnik2 import Projection
from mapnik2 import Box2d
from mapnik2 import Coord
from mapnik2 import CompositeOp

#from tile_pb2 import tile
from mercator import Mercator
	
import urllib
import StringIO
import ConfigParser 

#convert to independant format
import PIL.Image
import PIL.ImageOps

#returned results in this structure
from renderResult import RenderResult

#for NMS logging library
import mq_logging

METATILE = 8
TILE_SIZE = 256

def getTile( data ):
	theURL		= data[0]
	theSocket 	= urllib.urlopen( theURL )
	theData		= theSocket.read()
	theSocket.close()
	
	#mq_logging.debug("getTile Received %d bytes from tile request %s" % ( len( theData ), theURL ))

	return [ theData, data[1], data[2] ]

# this will be called each time a result is available
def print_result(request, result):
	result[0].append( result[1] )
#	print "**** Result from request #%s: %d" % (request.requestID, len( result[1] ) )

# this will be called when an exception occurs within a thread
# this example exception handler does little more than the default handler
def handle_exception(request, exc_info):
	if not isinstance(exc_info, tuple):
		# Something is seriously wrong...
		mq_logging.error(str(request))
		mq_logging.error(str(exc_info))
		raise SystemExit
	mq_logging.error("Exception occured in request #%s: %s" % (request.requestID, exc_info))
 
class Renderer :
    
   def __init__ ( self, configFileName ):
      self.concurrency = 16

      try:
         configSettings = ConfigParser.SafeConfigParser()
         configSettings.read( configFileName )

         #get the base url
         self.url = configSettings.get('vipinfo', 'url').replace('$', '%')
         self.concurrency = configSettings.getint('vipinfo', 'concurrency')

         #validate the url as best we can
         validated = self.url % {'z':'1', 'x':'2', 'y':'3'}

	 # one day there might be a parallel processing library for Python
	 # which works without problems and supports exceptions gracefully.
	 # today, i could not find it, if it exists...
	 #self.pool = Pool(self.concurrency)

      except Exception as detail:
         mq_logging.error('Could not load aerial/sat renderer configuration: %s' % (detail))
         raise

   def process(self, tile):
	z = str(tile.z)
	x_range = range( tile.x, tile.x + tile.dimensions[0] )
	y_range = range( tile.y, tile.y + tile.dimensions[1] )
	urls = [ [self.url % {'z':z, 'x':str(x), 'y':str(y)}, x - tile.x, y - tile.y] for x in x_range for y in y_range ]

	try:
		subTiles = map(getTile, urls)
	except Exception as detail:
		mq_logging.error('Could not read tile from aerial source: %s' % str(detail))
		raise
	
        if len( subTiles ) < tile.dimensions[0] * tile.dimensions[1] :
		mq_logging.warning("Result length too short, aborting")
		return None, None
        #
        #  make a composite image
        #
        features 	= None
        image		= PIL.Image.new( 'RGBA', tile.size )   
        
        for infoImage in subTiles:
            imageSubTile = PIL.Image.open( StringIO.StringIO( infoImage[0] ) )
            image.paste( imageSubTile, ( infoImage[1] * TILE_SIZE, infoImage[2] * TILE_SIZE ) )

        return RenderResult.from_image(tile, image)
    
    
