#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test coverage
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

import sys
import unittest
import logging


#this is for mqcoverage
sys.path.append('../')
import coverage.coveragemanager as coveragemanager
from coverage.CoverageChecker import CoverageChecker
from tile import Tile, xyFromLatLng
from mercator import Mercator
import RenderTask

class testCoverage(unittest.TestCase):
	def setUp(self):
		logging.basicConfig()
		#load the coverages
		self.coverageManager = coveragemanager.CoverageManager()
		self.coverageManager.load_coverage('../../../copyright_service_data/coverage/map/', False, 'map')
		self.coverageManager.load_coverage('../../../copyright_service_data/coverage/hyb/', False, 'hyb')
		self.coverageManager.load_coverage('../../../copyright_service_data/coverage/sat/', False, 'sat')
		self.coverageManager.load_coverage('../../../copyright_service_data/coverage/ter/', False, 'ter')

		#create a coverage checker
                config =        {
                                'map': '../../../copyright_service_data/coverage/map/',
                                'hyb': '../../../copyright_service_data/coverage/hyb/',
                                'sat': '../../../copyright_service_data/coverage/sat/',
                                'ter': '../../../copyright_service_data/coverage/ter/'
                                }
                self.checker = CoverageChecker(config)

		#locations whose coverages we should check
                self.locations = {}
		self.locations['north america'] = (40, -100)
		self.locations['south america'] = (-17, -55)
		self.locations['africa'] = (10, 20)
		self.locations['united kingdom'] = (55, -5)
		self.locations['western europe'] = (47, 5)
		self.locations['eastern europe'] = (50, 20)
		self.locations['asia'] = (45, 85)
		self.locations['ociana'] = (-25, 140)
		
		projection = Mercator(18+1)
		zooms = range(0, 19)
		#for each ll
		for name, location in self.locations.iteritems():
			tiles = []
			#through each zoom
			for zoom in zooms:
				#make a tile object
				x, y, z = xyFromLatLng(location, zoom, projection)
				tile = {"x": x, "y": y, "z": z, "gid": 0, "clientid": 0, "priority":0 , "style": 'map'}
				tiles.append(Tile(RenderTask.RenderTask(None, tile), projection))
			#overwrite the ll with the list of tiles per zoom level
			self.locations[name] = tiles


	def showResult(self, scale, projection, points, name):
		#get the coverage
		coverage = self.coverageManager.get_coverage(name, False)
		#see what datasets support this scale and projection
		candidateDataSets = coverage.getDataSetsForScale(scale, projection)
		#make a polygon with coordinates in (lng, lat) format
		polygonPoints = [[points[1], points[0]], [points[1], points[2]], [points[3], points[2]], [points[3], points[0]]] 
		#see which data sets this polygon intersects
		dataSetID = coverage.getIntersectingDataSets(candidateDataSets, polygonPoints, False, True)
		#get the data sets for that id
		dataSets = coverage.getDataSetsByID(dataSetID)
		print (name, dataSetID, dataSets)

	def testMap(self):
		#incoming request info
		scale = 54168
		projection = 'MERCATOR'
		points = [40.446947059600483, -73.828125, 40.713955826286046, -73.4765625]
		self.showResult(scale, projection, points, 'map')

	def testHyb(self):
		#incoming request info
		scale = 54168
		projection = 'MERCATOR'
		points = [40.446947059600483, -73.828125, 40.713955826286046, -73.4765625]
		self.showResult(scale, projection, points, 'hyb')

	def testSat(self):
		#incoming request info
		scale = 54168
		projection = 'MERCATOR'
		points = [40.446947059600483, -73.828125, 40.713955826286046, -73.4765625]
		self.showResult(scale, projection, points, 'sat')

	def testTer(self):
		#incoming request info
		scale = 54168
		projection = 'MERCATOR'
		points = [40.446947059600483, -73.828125, 40.713955826286046, -73.4765625]
		self.showResult(scale, projection, points, 'ter')

	def testChecker(self):
		#for each set of tiles per location
		for name, tiles in self.locations.iteritems():
			print '\n' + name
			#for each tile
			for tile in tiles:
				#check the coverage for this tile
				coverage = self.checker.check(tile, True)
				print coverage

	def testCheckerSubTile(self):
		#for each set of tiles per location
		for name, tiles in self.locations.iteritems():
			print '\n' + name
			#for each tile
			for tile in tiles:
				#check the coverage for this tile
				coverage, uniques = self.checker.checkSubTiles(tile, True)
				mixed = len(uniques) > 1 and 'OSM' in uniques
				print '%d : %s : %s' % (tile.z, 'Mixed' if mixed else 'Single', str(uniques))
				for key, value in sorted(coverage.iteritems()):
					print '   %d, %d: %s' % (key[0] + tile.x, key[1] + tile.y, str(value))

	#def tearDown(self):

#so that external modules can run this suite of tests
def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(testCoverage))
    return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())

