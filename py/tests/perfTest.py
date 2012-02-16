#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Performance Test for Renderers
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

import sys
import time
import math
sys.path.append( "../" )
from worker import loadConfig
import RenderTask
from tile import Tile, xyFromLatLng
from metacutter import cutFeatures
from mercator import Mercator

#for testing the renderers
class perfTest(unittest.TestCase):

	#some setup for the tests
	def setUp(self):
		#define output path
		self.outputPath = "results_performance/"
		
		#define lat, lon point for test area center
		self.testCenter = (38.901228, -77.038388)
		#self.testCenter = (39.739707, -104.985375)

		#load the items from the config
		config = ConfigParser()
		config.read('../../conf/worker.conf')
		self.storage, self.renderers, self.formats, self.format_args, self.coverage = loadConfig(config)
		self.projection = Mercator(18+1)
		logging.basicConfig(level=logging.DEBUG)

		#list of styles to test
		self.testStyles = ["hybus", "mapus"]

	#process one meta tile, based on tile coords, and record render time
	def timeTileRender(self, style, x, y, z):
		#get the renderer
		renderer = self.renderers.renderer_for(style)

		#create the tile
                tile = {"x": x, "y": y, "z": z, "gid": 0, "clientid": 0, "priority":0 , "style": style}
                tile = Tile(RenderTask.RenderTask(None, tile), self.projection)

		startTime = time.time()                

		#render the metatile
                result = renderer.process(tile)

		endTime = time.time()

		#return seconds taken to render
		return endTime - startTime

	#calculate average render time for a set of tiles for the given style and zoom level
	def timeZoomLevel(self, style, zoom):
		#get center tile
		x, y, z = xyFromLatLng(self.testCenter, zoom, self.projection)
		
		#increase test size based on zoom level
		testSize = int((zoom + 1) / 4) 
		
		#calculate first and last meta tile in range
		minMeta = (int(x / 8) - int(math.ceil(testSize / 2.0) - 1), int(y / 8) - int(math.ceil(testSize / 2.0) - 1))
		maxMeta = (int(x / 8) + int(testSize / 2), int(y / 8) + int(testSize / 2))
		
		#print (int(x / 8), int(y / 8)), testSize, minMeta, maxMeta
		
		tileCount = 0
		totalTime = 0
		minTime = 999999999
		maxTime = 0
		for my in range(minMeta[1], maxMeta[1] + 1):
			for mx in range(minMeta[0], maxMeta[0] + 1):
				renderTime = self.timeTileRender(style, mx * 8, my * 8, zoom)
				tileCount += 1
				totalTime += renderTime
				minTime = min(renderTime, minTime)
				maxTime = max(renderTime, maxTime)

		#return avg. render time, num tiles rendered, and total render time
		return (totalTime / tileCount if tileCount > 0 else 0), minTime, maxTime, tileCount, totalTime

	def writeResult(self, file, line):
		print line
		file.write(line + "\n")

	#test a mapware tile which should have transit pois
	def test_performance(self):
		filename = self.outputPath + "results-" + time.strftime("%Y-%m-%d-%H-%M-%S", time.gmtime()) + ".out"
		outfile = open(filename, "w", 0)

		self.writeResult(outfile, "Render Performance Results:")
                self.writeResult(outfile, "---------------------------")

		#iterate through zoom levels and styles
		for zoom in range(6, 19):
			self.writeResult(outfile, "\nZoom Level " + str(zoom) + ":")
			self.writeResult(outfile, "---------------")

			for style in self.testStyles:
				avgTime, minTime, maxTime, tileCount, totalTime = self.timeZoomLevel(style, zoom)
				self.writeResult(outfile, style + ": Avg " + str(avgTime) + " sec; Min " + str(minTime) + " sec; Max " + str(maxTime) + " sec; " + str(tileCount) + " tiles rendered; " + str(totalTime) + " sec total")

		outfile.close()		

#so that external modules can run this suite of tests
def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(perfTest))
    return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())
