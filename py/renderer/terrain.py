#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Terrain Renderer
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

#get our tile dimentions
import tile as dims

def getTile( theURL ):
	try:
		#mq_logging.debug("getTile tile request %s" % ( theURL ))
		theSocket 	= urllib.urlopen( theURL )
		theData		= theSocket.read()
		theSocket.close()

		#mq_logging.debug("getTile Received %d bytes from tile request %s" % ( len( theData ), theURL ))
		if theSocket.getcode() != 200:
			raise Exception(theSocket.getcode())
		elif theData == 'No tile found':
			raise Exception(theData)
		else:
			return theData
	except Exception as detail:
		mq_logging.error("getTile error fetching terrain image %s: %s" % ( theURL, str(detail) ))	
	return None
 
class Renderer :   
	def __init__( self, configFileName ):
		self.host = "mq-aerial-lm01.ihost.aol.com"
		self.port = 5005
		try:
			configSettings = ConfigParser.SafeConfigParser()
			configSettings.read( configFileName )
			configSection  = "vipinfo"
	
			self.host = configSettings.get( configSection, "host" )
			self.port = configSettings.getint( configSection, "port" )
			#landColor = configSettings.get( configSection, "color" )
	
			#keep a copy of a background image around
			#self.blank = PIL.Image.new('RGBA', (dims.METATILE*dims.TILE_SIZE, dims.METATILE*dims.TILE_SIZE), tuple(map(int, landColor.split(','))))
		except Exception as detail:
			mq_logging.error('Could not load terrain renderer configuration: %s' % (detail))
			raise

	def process(self, tile):
		#mq_logging.info(str(tile.bbox))
    	
		#http://mq-aerial-lm01.ihost.aol.com:8000/ter/z/x/y.png   NOTE:  file format ignored; PNG always returned
		theURL = "http://%s:%d/ter/%d/%d/%d.png" % ( self.host, self.port, tile.z, tile.x, tile.y )

		returnData = getTile( theURL )

		if returnData is None or len(returnData) < 1:
			return None, None

		try:
			image = PIL.Image.open( StringIO.StringIO( returnData ) )
		except Exception as detail:
			#use the land tile for places we dont have terrain
			#image = self.blank
			return None, None
		
		#hand back the image
		return RenderResult.from_image(tile, image)
 
    
