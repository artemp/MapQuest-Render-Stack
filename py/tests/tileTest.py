#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test tile interface
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

import sys
sys.path.append( "../" )
import RenderTask
from tile import Tile, xyFromLatLng, calculateScale 
from mercator import Mercator


#for testing the tile interface
class testTile(unittest.TestCase):

	#some setup for the tests
	def setUp(self):
		self.projection = Mercator(18+1)
		logging.basicConfig(level=logging.DEBUG)

	#test a mapware tile which should have transit pois
	def test_tile(self):
		#we'll go with zuerich
		ll = (47.376957, 8.539893)
		#for each zoom
		for z in range(0, 19):
			#get the tile coords
                        x, y, z = xyFromLatLng(ll, z, self.projection)
			#the tile we want
			tile = {'x': x, 'y': y, 'z': z, 'gid': 0, 'clientid': 0, 'priority':0 , 'style': 'map'}
			print 'input: ' + str(tile)
			#convert to mapware tile
			tile = Tile(RenderTask.RenderTask(None, tile), self.projection)
			print 'calc tile: ' + str(tile)
			tile.scale = calculateScale(tile)
			print 'calc scale: ' + str(tile)

	#dont need this method for now
	#def tearDown:

#so that external modules can run this suite of tests
def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(testTile))
    return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())
