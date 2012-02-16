#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Removes features with no geometry
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
import getopt
import osgeo.ogr as ogr

def Usage():
	print>>sys.stderr,'Usage:'
        print>>sys.stderr,'-i=input\t\tThe shape file to be cleaned'
        print>>sys.stderr,'-o=output\t\tThe output shape file. By default the original file names are used and placed in a directory named \'clean\''

#grab the value for a parameter that was passed in
def GetParam(optionName, options, inputs, default=None):
	try:
		value = inputs[options.index(optionName)]
		return value if value != '' else default
	except:
		return default

def CleanShape(oldPath, newPath):
	#load the shape
	oldShape = ogr.Open(oldPath, True)
	#make a new shape
	newShape = oldShape.GetDriver().CreateDataSource(newPath)

	#for each layer in the shape
	for layerIndex in range(0, oldShape.GetLayerCount()):
		#get the layer
		oldLayer = oldShape.GetLayerByIndex(layerIndex)
		#make a duplicate
		newLayer = newShape.CreateLayer(oldLayer.GetName())
		#for each feature in the layer
		feature = oldLayer.GetNextFeature()
		while feature is not None:
			#get the geometry
			geometry = feature.GetGeometryRef()
			#if there isn't any geometry for this feature
			if geometry is None or geometry.IsEmpty(): #or geometry.IsValid() == False: #geometry.GetGeometryCount() < 1:
				#remove the feature from the layer
				print>>sys.stdout,'Feature "%d" was removed from layer "%s"' % (feature.GetFID(), oldLayer.GetName())
			#keep this one
			else:
				newLayer.CreateFeature(feature)
			#get the next feature
			feature = oldLayer.GetNextFeature()
		#write changes to disk
		if newLayer.SyncToDisk() != 0:
			print>>sys.stderr,'Changes to layer "%s" could not be synced to disk' % (newLayer.GetName())
		else:
			print>>sys.stdout,'Changes to layer "%s" successfully synced to disk' % (newLayer.GetName())

	#finshed with the datasources
	newShape = None
	oldShape = None

if __name__ == '__main__':

	try:
		#no long arguments, too lazy to implement for now...
		opts, args = getopt.getopt(sys.argv[1:], "i:o:", [])
		options = [option[0] for option in opts]
		inputs = [input[1] for input in opts]
		required = set(['-i'])
		if required.intersection(options) != required:
			raise getopt.error('Usage', 'Missing required parameter')
	except getopt.GetoptError:
		Usage()
		sys.exit(2)	

	#clean the file and save the output
	try:
		CleanShape(GetParam('-i', options, inputs), GetParam('-o', options, inputs, 'clean'))
	except Exception as detail:
		print>>sys.stderr,'Exception: %s' % (str(detail))
