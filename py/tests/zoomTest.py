#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test getting tiles
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
from ConfigParser import ConfigParser

import sys
sys.path.append( "../" )
import RenderTask
from tile import Tile, xyFromLatLng 
from mercator import Mercator
from worker import loadConfig
from time import time

#for testing tiles at each zoom level for a single location
class testZoom(unittest.TestCase):

	#some setup for the tests
	def setUp(self):
		#load the items from the config
		config = ConfigParser()
		config.read('../../conf/worker.conf')
		self.storage, self.renderers, self.formats, self.format_args, self.coverage = loadConfig(config)
		self.projection = Mercator(18+1)
		logging.basicConfig(level=logging.DEBUG)

	#get a for each zoom raster a tile
	def getTiles(self, renderer, ll, zooms):
		#get a tile at each zoom level for Berlin
		for zoom in zooms:
			#get the tile coords
			x, y, z = xyFromLatLng(ll, zoom, self.projection)
			#make a job
			job = {"x": x, "y": y, "z": z, "gid": 0, "clientid": 0, "priority":0 , "style": 'map'}
			#convert it to a tile
			tile = Tile(RenderTask.RenderTask(None, job), self.projection)
			#rasterize the tile
			try:
				t = time()
				image, meta = renderer.process(tile)
				sec = time() - t
				print '%s took %s seconds' % ((x, y, z), sec)
			except Exception as detail:
				print 'failed to get %s with exception %s' % (job, detail)

	#test mapnik
	def test_mapnik(self):
		#get the whole stack of Berlin tiles for each zoom level
		#self.getTiles(self.renderers[('mapnik', 'map')], (52.517256, 13.40638), range(8, 19))
		#get the whole stack of New York tiles for each zoom level
		self.getTiles(self.renderers[('mapnik', 'map')], (40.721580, -73.997555), range(0, 19))

	#test mapware
	def test_mapware(self):
		#get the whole stack of Berlin tiles for each zoom level
		#self.getTiles(self.renderers[('mapnik', 'map')], (52.517256, 13.40638), range(8, 19))
		#get the whole stack of New York tiles for each zoom level
		self.getTiles(self.renderers[('mapware', 'map')], (40.721580, -73.997555), range(0, 19))


	#dont need this method for now
	#def tearDown:

#so that external modules can run this suite of tests
def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(testZoom))
    return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())
