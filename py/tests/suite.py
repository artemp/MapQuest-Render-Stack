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

#get all the tests
import coverageTest
import metaCutterTest
import rendererTest
import tileTest
import transcodeTest
import zoomTest


if __name__ == '__main__':
	#coverage test
	unittest.TextTestRunner(verbosity=2).run(coverageTest.suite())
	#meta cutter test
	unittest.TextTestRunner(verbosity=2).run(metaCutterTest.suite())
	#tile test
	unittest.TextTestRunner(verbosity=2).run(tileTest.suite())
	#transcoder test
	unittest.TextTestRunner(verbosity=2).run(transcodeTest.suite())
	#renderer test
	unittest.TextTestRunner(verbosity=2).run(rendererTest.suite())
	#zoom test
	unittest.TextTestRunner(verbosity=2).run(zoomTest.suite())
