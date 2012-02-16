#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test tile interface
#
#  Author: john.novaj@mapquest.com
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

import mercator

from tile import Tile, xyFromLatLng, calculateScale 
import RenderTask
from renderer.aerial import Renderer

import PIL.Image
import PIL.ImageOps

#for testing the tile interface
class testTile(unittest.TestCase):

   #some setup for the tests
   def setUp(self):
      self.projection = mercator.Mercator( 19 )
      logging.basicConfig(level=logging.DEBUG)

	#test a mapware tile which should have transit pois
   def test_tile(self):
      #we'll go with saramento (west coast bias)
      ll = (38.555556, -121.468889)
      #for each zoom
      for z in range(0, 19):
         #get the tile coords
         x, y, z = xyFromLatLng(ll, z, self.projection)
         print "Tile info", x, y, z
         #the tile we want
         tile = {'x': x, 'y': y, 'z': z, 'gid': 0, 'clientid': 0, 'priority':0 , 'style': 'map'}
         print 'input: ' + str(tile)
         #convert to mapware tile
         tile = Tile(RenderTask.RenderTask(None, tile), self.projection)
         print 'calc tile: ' + str(tile)
         tile.scale = calculateScale(tile)
         print 'calc scale: ' + str(tile)

   #test a mapware tile which should have transit pois
   def test_tileFetch(self):
      #we'll go with saramento (west coast bias)
      ll = (38.555556, -121.468889)
      #for each zoom
      for z in range(10, 15):
         #get the tile coords
         x, y, z = xyFromLatLng(ll, z, self.projection)
         print "Tile info", x, y, z
         #the tile we want
         tile = {'x': x, 'y': y, 'z': z, 'gid': 0, 'clientid': 0, 'priority':0 , 'style': 'map'}
         print 'input: ' + str(tile)
         #convert to mapware tile
         tile = Tile(RenderTask.RenderTask(None, tile), self.projection)

         renderer = Renderer( "../../conf/dc_sat.conf", None, "sat" )
         results = renderer.process( tile )

         results[0].save( "test_%0d_%0d_%0d.jpg" % ( tile.x, tile.y, tile.z ), "jpeg" )
			
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
