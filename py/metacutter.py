#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Meta Data Cutting
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

#for rectangle clipping math
import math
#for doing geojson features
from geojson import FeatureCollection, Feature, MultiPolygon, dumps
#for NMS logging library
import mq_logging

#used to create static member functions
class Callable:
	def __init__(self, anycallable):
		self.__call__ = anycallable

'''expects MAPWARE formatted poi data'''
#given poi data return a geojson feature collection of poi data that was rasterized into the mapware tile
def extractFeaturesMW(metaData):
	#to hold the features
	features = FeatureCollection([])

	#plugin didn't return anything
	if metaData is None:
		return features

	try:
		#for each poi in the meta data
		for poi in metaData['pois']:
			#try to get the id, icon, label, and name
			try:
				#add the positions of the rectangles to the multi polygon
				#format is: multipolygon[ polygon[ linestring[position[], position[]], linestring[position[], position[]] ]]
				label = poi['label']
				icon = poi['icon']
				#create the multipolygon
				geometry = MultiPolygon([ [[ [label['x1'], label['y1']], [label['x2'], label['y1']], [label['x2'], label['y2']], [label['x1'], label['y2']] ]],\
				[[ [icon['x1'], icon['y1']], [icon['x2'], icon['y1']], [icon['x2'], icon['y2']], [icon['x1'], icon['y2']] ]] ])
				#throws an exception if id and name aren't in the poi
				features.features.append(Feature(poi['id'], geometry, {'name': poi['name'], 'type': 'poi'}))
			#if there is a problem ignore this one
			except Exception as exception:
				#print out bad entries
				mq_logging.warning('metacutter cant get poi: %s: %s' (exception, poi))
	#didn't find the attribute pois
	except Exception as exception:
		#print out bad poi data
		mq_logging.error('metacutter cant get attribute poi: %s: %s' (exception, str(metaData)))

	#hand them back
	return features

'''expects MAPNIK formatted poi data'''
#given poi data return a geojson feature collection of poi data that was rasterized into the mapnik tile
def extractFeaturesMNK(metaData):
	#to hold the features
	featureCollection = FeatureCollection([])

	try:
		#for each rasterized object
		for poi in metaData:
			#try to get the id, icon, label, and name
			try:
				#expand the box to include the subpixel portion of the region
				box = (int(poi.box[0]), int(poi.box[1]), int(poi.box[2]) + 1, int(poi.box[3]) + 1)
				#create the multipolygon
				geometry = MultiPolygon([ [[ [box[0], box[1]], [box[2], box[1]], [box[2], box[3]], [box[0], box[3]] ]] ])
				#throws an exception if id and name aren't in the poi
				poi = dict(poi.properties)
				#WARNING: efficiency hack: mapnik search plugin returns features sorted by their search id we need only look at previous feature
				id = int(poi['id'])
				previous = len(featureCollection.features) - 1
				#if the previous feature was this same one
				if previous > -1 and featureCollection.features[previous].id == id:
					featureCollection.features[previous].geometry.coordinates.append(geometry.coordinates[0])
				#make a new feature
				else:
					featureCollection.features.append(Feature(id, geometry, {'name': poi['name'], 'type': 'poi'}))
			#if there is a problem ignore this one
			except Exception as exception:
				#print out bad entries
				mq_logging.warning('metacutter cant get poi: %s: %s' (exception, poi))
	#didn't find the attribute pois
	except Exception as exception:
		#print out bad poi data
		mq_logging.warning('metacutter cant get attribute poi: %s: %s' (exception, metaData))

	#hand them back
	return featureCollection

#given a rect and the pixels (width, height), dimensions (rows,columns) of a grid
#returns a list of rects clipped to the grid's cells
def clipGeometry(pixels, dimensions, geometry):
	#no clipped rects yet
	geometries = {}
	width = pixels[0]
	height = pixels[1]

	try:
		#only support multipolygon for now
		if geometry.type == 'MultiPolygon':
			#get each polygon
			for polygon in geometry.coordinates:
				#TODO: unhack to support all geometries and real polygons instead of just assuming rect
				#since each polygon is defined as an outer ring and a bunch of inner rings we just grab the outer
				polygon = polygon[0]
				rect = (polygon[0][0], polygon[0][1], polygon[2][0], polygon[2][1])
				#skip degenerate polygons
				if rect[3] - rect[1] == 0 and rect[2] - rect[0] == 0:
					continue
				#for each sub tile that this rect overlaps
				for r in range(int(math.floor(rect[1] / height)), int(math.floor(rect[3] / height)) + 1):
					for c in range(int(math.floor(rect[0] / width)), int(math.floor(rect[2] / width)) + 1):
						#not keeping these
						if r < 0 or r >= dimensions[0] or c < 0 or c >= dimensions[1]:
							continue
						#get the tile's pixel extents
						tile = (width * c, height * r, width * (c + 1), height * (r + 1))
						#get the upper left
						x0 = rect[0] - tile[0] if rect[0] > tile[0] else 0
						y0 = rect[1] - tile[1] if rect[1] > tile[1] else 0
						#get the lower right
						x1 = rect[2] - tile[0] if rect[2] < tile[2] else width - 1
						y1 = rect[3] - tile[1] if rect[3] < tile[3] else height - 1
						#just need to add another polygon to this multipolygon
						if (r, c) in geometries:
							geometries[(r, c)].coordinates.append([[ [x0, y0], [x1, y0], [x1, y1], [x0, y1] ]])
						else:
							geometries[(r, c)] = MultiPolygon([[[ [x0, y0], [x1, y0], [x1, y1], [x0, y1] ]]])
	#something wasn't there						
	except Exception as exception:
		mq_logging.error('problem parsing geometry')

 	#return the features
	return geometries

#given a dictionary where the search id is the key and the values are geojson features (containing polygons)
#and the pixels (width, height) of the tile and dimensions (rows, columns) of the master tile is divided into
#returns a dictionary where the keys are tuples (row, column) and the values are geojson feature collections
def cutFeatures(features, pixels, dimensions, dump = False):
	#to hold the cut features
	cutFeatures = {}

	#invalid input
	if pixels[0] < 1 or pixels[1] < 1 or dimensions[0] < 1 or dimensions[1] < 1:
		return cutFeatures

	#figure out how big the sub tiles will be
	width = pixels[0] / dimensions[1]
	height = pixels[1] / dimensions[0]

	#invalid input
	if width < 1 or height < 1:
		return cutFeatures

	#make sure every sub tile has a blank feature collection
	for y in range(dimensions[0]):
		for x in range(dimensions[1]):
			cutFeatures[(y, x)] = FeatureCollection([])

	#for each feature
	if features is not None:
		for feature in features.features:
			#to keep each clipped geometry
			clippedGeometries = clipGeometry((width, height), dimensions, feature.geometry)
			#save them to their respective meta tiles
			for location, clippedGeometry in clippedGeometries.iteritems():
				#add it to the feature collection
				cutFeatures[location].features.append(Feature(feature.id, clippedGeometry, feature.properties))

	#serialize them
	if dump is True:
		for location, featureCollection in cutFeatures.iteritems():
			cutFeatures[location] = dumps(featureCollection)

	#hand them back
	return cutFeatures
