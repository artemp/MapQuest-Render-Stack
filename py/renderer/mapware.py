#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  MapWare Renderer
#
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

#for wgetting a url
import urllib2
#for parsing search results
import json
#for getting tiles from mapware (c++ module)
import mapwareTiler
#for cutting up poi data
import metacutter
#for saving the image data
from PIL import Image
import StringIO

#returned results in this structure
from renderResult import RenderResult

#for new map stack logging
import mq_logging

#mapware dpi is always assumed to be 72
DPI = 72

#mapware configs and styles
class MapWareConfig:

	#constructor
	def __init__(self, configFileName):
		#parse the config file
		configFile = open(configFileName, "r")
	        config = json.load(configFile)
		configFile.close()

		#get the servers
		try:
			self.name = str(config['name'])
			self.mapServer = None
			self.searchServer = None
			#for each server in the config
			for server in config['servers']:
				#ignore any that are missing parameters we need
				if 'name' in server and 'server' in server and 'port' in server and 'clientId' in server:
					name = server['name']
					if name == 'map':
						self.mapServer = server
					elif name == 'search':
						self.searchServer = server
					else:
						print >> stderr, 'Ignoring unrecognized: %s' % (server)
				else:
					mq_logging.warning('Ignoring malformed: %s' % (server))
			#check if we were able to get the required servers
			if self.searchServer is None or self.mapServer is None:
				raise Exception('search or map servers missing or misconfigured')
		except Exception as detail:
			mq_logging.error('Invalid MapWare server configuration: %s' % (detail))
			raise

		#get the styles
		self.styles = []
		try:
			#for each style in the config
			for style in config['styles']:
				#ignore any that are missing parameters we need
				if 'name' in style and 'dt' in style and 'style' in style and 'search' in style and 'zoom' in style:
					self.styles.append(style)
				else:
					mq_logging.warning('Ignorning malformed: %s' % (style))
		except Exception as detail:
			mq_logging.error('Invalid MapWare style configuration: %s' % (detail))
			raise
	        
	def __repr__(self):
		return '<MapWareConfig searchServer:%s mapServer:%s styles:%s>' % (self.searchServer, self.mapServer, self.styles)

#mapware tile getter
class Renderer:

	#constructor
	def __init__(self, configFileName):
		#load the config file (with tile/search servers and style info)
		self.config = None
		try:
			self.config = MapWareConfig(configFileName)
		except Exception as detail:
			mq_logging.error('Could not load mapware renderer configuration: %s' % (detail))
                        raise

	        #make the initial object with the server that you'll be getting tiles from
		#server, port, clientID, path='mq'
		self.tiler = mapwareTiler.MapWareTiler(str(self.config.mapServer['server']), self.config.mapServer['port'], str(self.config.mapServer['clientId']))

		#keep search base urls with dt as the key
		self.searches = {}

		#add all the styles for the different types of pois we are going to inject
		for style in self.config.styles:
			#if this style requires a search for feature data
			if style['search'] is not None:
				#form the url, just missing the map state of course
				self.searches[style['dt']] = ''.join((':'.join((self.config.searchServer['server'],
									str(self.config.searchServer['port']))),
									style['search'], '&clientId=',
									str(self.config.searchServer['clientId']),
									'&boundingbox='))

	#the coverages are determined on the server side for mapware
	def add_region(self, name, mapfile, wkt_mask_file):
		#self.regions = []
		return None

	#checks which style to use based on what part of the world the tile is in
	#mapware uses the coverage switcher on the server side to do this so we don't have to
	def _check_region(self, bbox):
		return None

	#get all the pois and save them
	def stylize(self, tile):
		#get an http client
		opener = urllib2.build_opener()

		#clear previous pois
		self.tiler.ClearPOIs()
		#clear previous style strings
		self.tiler.ClearStyleStrings()

		#for each style
		for style in self.config.styles:
			#skip this style string if it doesn't apply to this zoom level
			if tile.z not in style['zoom']:
				continue;
			
			#add the dt and style string
			self.tiler.AddStyleString(style['dt'], str(style['style']))

			#skip the rest if we don't need to add features from search
			if style['search'] is None:
				continue

			#get the base url
			url = ''.join((self.searches[style['dt']],
			str(tile.bbox[0][0]),',',str(tile.bbox[0][1]),',',str(tile.bbox[1][0]),',',str(tile.bbox[1][1])))

			#load the page
			response = opener.open(url)
			#parse the json response into a dict
			searchResults = json.load(response)
			#done with it
			response.close()

			#parse out the pois and put them in the tiler
			try:
				for permutation in searchResults['permutations']:
					search = permutation['search']
					for poi in search['results']:
						try:
							#skip anything without address point geocode P1
							if poi['GeoAddress']['ResultCode'].find("P1") == -1:
								continue
							#add the poi, will throw exception if poi is missing some data and will just go to next one
							ll = poi['GeoAddress']['DisplayLatLng']
	                                                self.tiler.AddPOI(style['dt'], str(poi['name']), str(poi['id']), float(ll['Lat']), float(ll['Lng']))
						except:
							continue
			except Exception as detail:
				#mq_logging.warning('Poi parse failed: %s' % (detail))
				continue

	#get a tile and optionally some meta data
	def process(self, tile):
		#mapware requires an image format
		tile.format = mapwareTiler.ImageType.RAW;
		#start with no image and no meta data
		image = None #Image.new('RGBA', tile.size)
		features = None
		#setup the tile that you want to get
		#width, height, scale, lat, lng, projection, dpi
		self.tiler.SetMapState(tile.size[0], tile.size[1], tile.scale, tile.center[0], tile.center[1], 'Proj:Mercator', DPI)

		#set the style we want to use
		self.tiler.SetStyleName(self.config.name)

		#fetch the pois for each style type
		self.stylize(tile)

		#call out to mapware to get the tile
		try:
			#inject the pois into the tile and get the tile/metadata back
			#image type, return meta data
			result = self.tiler.GetTile(tile.format, True)

			#if there are failure messages
			if len(result.failureMessages) > 0:
				raise Exception(result.failureMessages)

			#if you are using an encoded image type (ie. with a header)
			if tile.format is not mapwareTiler.ImageType.RAW:
				bytes = StringIO.StringIO()
				bytes.write(result.tostring())
				bytes.write('\0')
				bytes.seek(0)
				image = Image.open(bytes)
			#the format is just raw RGBA data
			else:
				image = Image.frombuffer('RGBA', tile.size, result.tostring(), 'raw', 'RGBA', 0, 0)

			#get the boundign boxes and ids out of the mapware data
			features = metacutter.extractFeaturesMW(json.loads(result.metaData))

		except Exception as detail:
			mq_logging.error('MapWare tile fetch failed: %s' % (detail))
			raise

		#give back the image and the meta data
		return RenderResult.from_image(tile, image, features)
