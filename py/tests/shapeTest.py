#!/usr/bin/env python

try:
    from osgeo import osr
    from osgeo import ogr
    from osgeo import gdal
except ImportError:
    import osr
    import ogr

import os
import sys

from MQBB import *
from gdal_file_info import *

class MQShapeFile:

	def __init__( self, shapefile ):
		self.shape = shapefile
		self.theExtent = MQBB()
		
#------------------------------------------------------------------------------
	def computeTileIntersection( self ):
		ds = ogr.Open( self.shape )

		if ds is None:
			print "open of shape file failed"
			return None

		lyr = ds.GetLayer( 0 )

		ring = ogr.Geometry( type = ogr.wkbLinearRing )
		ring.AddPoint( self.theExtent.x1, self.theExtent.y1 )
		ring.AddPoint( self.theExtent.x2, self.theExtent.y1 )
		ring.AddPoint( self.theExtent.x2, self.theExtent.y2 )
		ring.AddPoint( self.theExtent.x1, self.theExtent.y2 )
		ring.AddPoint( self.theExtent.x1, self.theExtent.y1 )

		poly = ogr.Geometry( type = ogr.wkbPolygon )
		poly.AddGeometryDirectly( ring )

		lyr.SetSpatialFilter( poly )
		lyr.ResetReading()
    
		count = lyr.GetFeatureCount()

		self.arrTiles = []

		for feat in lyr:
			feat_defn = lyr.GetLayerDefn()

			for i in range(feat_defn.GetFieldCount()):
				field_defn = feat_defn.GetFieldDefn(i)

				if field_defn.GetType() == ogr.OFTString and field_defn.GetName() == 'location':
					self.arrTiles.append( feat.GetFieldAsString(i) )
    #                print field_defn.GetName()

		lyr    = None
		ds    = None

		return self.arrTiles
        
#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
if __name__ == "__main__":
	bVerbose	= True 
	
	objShape = MQShapeFile( "/Users/johnnovak00/L5.shp" )
	
	inputExtent = [ -12523442.7142, 3757032.81427, -11271098.4428, 5009377.0857 ]
	objShape.theExtent.set( inputExtent[0], inputExtent[1], inputExtent[2], inputExtent[3]  )
	arrTiles =  objShape.computeTileIntersection(  )
	
	print "Input extent", inputExtent
	print "Matching tile count", len( arrTiles )
	
	for aTile in arrTiles:
		print aTile
		
		if bVerbose:
			theFileInfo = gdal_file_info( aTile )
			theFileInfo.reportExtent()
			print
			
	
