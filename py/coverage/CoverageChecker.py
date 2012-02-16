#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Coverage checking utility wrapper
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
import logging
import coveragemanager

class CoverageChecker:
	def __init__(self, config):
		#this silences a warning about some logging code deep with in the dts coverage stuff
		logging.basicConfig()
		#load the coverages
		self.coverageManager = coveragemanager.CoverageManager()
		#load each coverage
		for type, location in config.iteritems():
			self.coverageManager.load_coverage(location, False, type)
		#self.coverageManager.load_coverage('../../../copyright_service_data/coverage/map/', False, 'map')

	#returns a list of coverages for a meta tile
	def check(self, tile, allCoverages = False):
		#get the coverage
		coverage = self.coverageManager.get_coverage(tile.style, False)
		#see what datasets support this scale and projection
		candidateDataSets = coverage.getDataSetsForScale(tile.scale, 'MERCATOR')
		#make a polygon with coordinates in (lat,lng) format
		polygon =[	[tile.bbox[0][0], tile.bbox[0][1]],
				[tile.bbox[1][0], tile.bbox[0][1]],
				[tile.bbox[1][0], tile.bbox[1][1]],
				[tile.bbox[0][0], tile.bbox[1][1]]]
		#see which data sets this polygon intersects
		dataSetIDs = coverage.getIntersectingDataSets(candidateDataSets, polygon, allCoverages, True)
		#get the data sets for that id
		dataSets = [dataSet for dataSetID in dataSetIDs for dataSet in coverage.getDataSetsByID(dataSetID)]
		#return a comprehension of coverage names
		#return [dataSet.vendor_name for dataSet in dataSets]
		return [dataSet.getCoverageName() for dataSet in dataSets]

	#returns a dictionary in which the keys are tile y, x tuples and
	#values are lists of coverage names for the respective tile
	#also returns a set of unique coverages for this metatile
	def checkSubTiles(self, tile, allCoverages = False):
		coverages = dict()
		uniqueCoverages = set()
		#for each tile
		for y in range(0, tile.dimensions[0]):
			for x in range(0, tile.dimensions[1]):
				#get the subtile
				subTile = tile.getSubTile(y, x)
				#get its coverage
				names = self.check(subTile, allCoverages)
				#keep track of which unique coverages we see
				if len(names) > 0:
					uniqueCoverages.add(names[0])
				else:
					uniqueCoverages.add(None)				
				#save the coverage
				coverages[(y, x)] = names

		#return the dict of coverages per sub tile and the list of unique ones
		return coverages, uniqueCoverages
