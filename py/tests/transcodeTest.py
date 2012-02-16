#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Test transcoding images
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
from PIL import Image
from time import time
import sys

sys.path.append( "../" )
from worker import loadConfig
from transcode import Transcode


#for testing transcoding images
class testTranscoder(unittest.TestCase):

	#some setup for the tests
	def setUp(self):
		#load the items from the config
		config = ConfigParser()
		config.read('../../conf/worker.conf')
		self.storage, self.renderers, self.formats, self.format_args, self.coverage = loadConfig(config)
		self.hyb = Image.open('./hyb.png')
		self.map = Image.open('./map.png')
		logging.basicConfig(level=logging.DEBUG)

	#save the map image in each format
	def test_map(self):
		t = time()
		formats = [f for f in self.format_args.keys() if f != 'json']
		tiles = Transcode(self.map, 8, formats, self.format_args)
		sec = time() - t
		print '%s took %s seconds' % (formats, sec)

	#save the map image in each format
	def test_hyb(self):
		t = time()
		formats = [f for f in self.format_args.keys() if f != 'json']
		tiles = Transcode(self.hyb, 8, formats, self.format_args)
		sec = time() - t
		print '%s took %s seconds' % (formats, sec)

	#dont need this method for now
	#def tearDown:

#so that external modules can run this suite of tests
def suite():
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(testTranscoder))
    return suite

#run the unit tests
#by deriving the testRenderer class from unittest.TestCase
#and then calling unittest.main it looks for any methods named
# test* and runs them
if __name__ == "__main__":
	unittest.TextTestRunner(verbosity=2).run(suite())
