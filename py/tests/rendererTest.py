#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test renderers
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

import unittest
import logging
from PIL import Image, ImageDraw
import PIL.ImageOps
from ConfigParser import ConfigParser

import os
import sys
sys.path.append( "../" )
sys.path.append( "../../" )
from worker import loadConfig
import RenderTask
from tile import Tile, xyFromLatLng
from metacutter import cutFeatures
from mercator import Mercator

#for testing the renderers
class testRenderer(unittest.TestCase):

	#some setup for the tests
	def setUp(self):
		#load the items from the config
		config = ConfigParser()
		config.read('../../conf/worker.conf')
		self.storage, self.rendererFactory, self.formats, self.format_args, self.coverage, memlimit = loadConfig(config)
		self.projection = Mercator(18+1)
		logging.basicConfig(level=logging.DEBUG)

	#cut up the sub tiles and draw the clickable regions on them
	def drawClickRegions(self, image, meta, dimensions, invert):
		negative = None
		drawer = None
		#get the negative
		if invert is True:
			negative = PIL.ImageOps.invert(image)
		#for drawing rectangles
		else:
			drawer = ImageDraw.Draw(image)
		#figure out the sub tile size
		height = image.size[1] / dimensions[0]
		width = image.size[0] / dimensions[1]
		#for each meta sub tile and its pois
		for location, featureCollection in meta.iteritems():
			#for each feature
			for feature in featureCollection.features:
				#for each polygon in this feature
				for polygon in feature.geometry.coordinates:
					#we don't support holes in our polygons
					polygon = polygon[0]
					#get the sub tile region that is clickable
					#location is a column major tuple for the sub tile location
					region = (location[0] * width + polygon[0][0], 
							location[1] * height + polygon[0][1],
							location[0] * width + polygon[2][0],
							location[1] * height + polygon[2][1])
					#invert the image where there are clickable spots
					if invert is True:
						#+1 because we need range inclusive of start and end coordinates
						region = (region[0], region[1], region[2] + 1, region[3] + 1)
						#cut out the negative
						clickImage = negative.crop(region)
						#paste over the original
						image.paste(clickImage, region)
					#draw red rectangles where there are clickable spots
					else:
						#draw the lines
						drawer.line((region[0], region[1], region[2], region[1]), fill = (255, 0, 0))
						drawer.line((region[2], region[1], region[2], region[3]), fill = (255, 0, 0))
						drawer.line((region[2], region[3], region[0], region[3]), fill = (255, 0, 0))
						drawer.line((region[0], region[3], region[0], region[1]), fill = (255, 0, 0))
		#return the image
		return image

	#save a jpeg
	def saveJPG(self, image, name):
		image.save(name, 'jpeg', quality=85, progressive=True, optimize=True)
	
	#save a full color png
	def savePNG(self, image, name):
		image.save(name, 'png', optimize=True)

	#save a palettized png
	def savePNG256(self, image, name):
		#get the alpha channel
		alpha = image.split()[3]
		#quantize to 255 colors leaving one for transparent
		image = image.convert('RGB').convert('P', palette=PIL.Image.ADAPTIVE, colors=255)
		#anything less than 1/4 opaque will be transparent (aliased but this is the best we can do)
		mask = PIL.Image.eval(alpha, lambda a: 255 if a <= 64 else 0)
		#use the 256th color from the palette for all the on-pixels in the mask
		image.paste(255, mask)
		#set the transparency index
		image.save(name, 'png', optimize=True, palette=True, transparency=255)

	#save a gif
	def saveGIF(self, image, name):
		#get the alpha channel
		alpha = image.split()[3]
		#quantize to 255 colors leaving one for transparent
		image = image.convert('RGB').convert('P', palette=PIL.Image.ADAPTIVE, colors=255)
		#anything less than 1/4 opaque will be transparent (aliased but this is the best we can do)
		mask = PIL.Image.eval(alpha, lambda a: 255 if a <= 64 else 0)
		#use the 256th color from the palette for all the on-pixels in the mask
		image.paste(255, mask)
		#set the transparency index
		image.save(name, 'gif', palette=True, transparency=255)

	#give back tile and job
	def makeJobTile(self, ll, zoom, style):
		#create a tile
                x, y, z = xyFromLatLng(ll, zoom, self.projection)
                tile = {"x": x, "y": y, "z": z, "gid": 0, "clientid": 0, "priority":0 , "style": style}
                job = RenderTask.RenderTask(None, tile)
                tile = Tile(job, self.projection)
		return job, tile

	#test a tile which should have transit pois
	def test_tile(self):
		#create a tile
		job, tile = self.makeJobTile((4.650748, -74.353379), 15, 'map')
		
		#get the renderer
		renderer = self.rendererFactory.renderer_for(tile.style)
		
		#make and save the first run
		result = renderer.process(tile)
		self.saveJPG(result.data[(0,0)], 'mnk.1.jpg')
		size1 = os.path.getsize('mnk.1.jpg')

		for i in range(1000):

			#get the tile and the meta data
			result = renderer.process(tile)

			#TODO: take multiple images in dict and turn them into one pil image
			#for y in range(tile.dimensions[1]):
			#	for x in range(tile.dimensions[0]):

			#save it
			self.saveJPG(result.data[(0,0)], 'mnk.2.jpg')
			size2 = os.path.getsize('mnk.2.jpg')
			if size1 != size2:
				raise Exception('sizes: %d !=  %d' % (size1, size2))
			if i % 50 == 0:
				print 'done %d' % i

		#visualize the clickable regions
		#image = self.drawClickRegions(result.data, result.meta, tile.dimensions, False)
		#self.saveGIF(result.data, 'mnk_click.gif')

		#for each sub meta tile show the json
		#print 'Sub Tiles:'
		#for key, value in sorted(result.meta.iteritems()):
		#	print '%s: %s\n' % (str(key), value)

	#dont need this method for now
	#def tearDown:

#so that external modules can run this suite of tests
def suite():
	suite = unittest.TestSuite()
	suite.addTest(unittest.makeSuite(testRenderer))
	return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())
