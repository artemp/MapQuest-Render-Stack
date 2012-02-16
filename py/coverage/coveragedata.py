'''Consists of coverage class and its methods
'''
#TODO replace string with new non-deprecated lib
from coverageconstants import COVERAGE_XML_FILENAME
from mqdataset import MQDataSet
from shapely.geometry import Point, Polygon, asPoint, asPolygon
from xml.etree.ElementTree import parse
from xml.parsers.expat import ExpatError as ParseError


import traceback

import os
import logging
import string
from shapefile import shapefile

LOG = logging.getLogger(__name__)

class Coverage():
    ''' 
        Coverage class loads the datasets and checks for coverage
    '''
    version_location = ''
    coverage = []

    def __init__(self, tmp_version_location):
        ''' 
            Initializes coverage datasets and stores info w.r.t to coverage being loaded
        '''
#        traceback.print_tb()
        
        self.dictDatasets	= {}
        
        self.version_location = tmp_version_location
        self.xml_file_name = self.version_location + COVERAGE_XML_FILENAME
        self.xml_file = None 
        self.load_coverage_xml()
        
		
    def load_coverage_xml(self):
        self.xml_file = open(self.version_location + COVERAGE_XML_FILENAME, 'r')
        xmldata = None
        try:
            xmldata = parse(self.xml_file)
        except ParseError:
            raise
        else:
            self.xml_file.close() 
        
        self.ingestShapefiles()
        self.process_coverage_data(xmldata, self.version_location)
        
    def dump(self):
        ''' Dumps the datasets present in the coverage '''
        for dataset in self.coverage:
            LOG.debug('coverage : ' + str(dataset.dump()))
        
    def clear(self):
        ''' Clears the coverage data '''
        self.coverage = []
        
    def reload(self):
        self.load_coverage_xml()

    def ingestShapefiles( self ):
		''' 
            Ingest any shapefiles in the coverage directory.
            
            Store in a dict keyed by dataset name and use this data instead of .vp files.
		'''
		for root, dirs, files in os.walk( self.version_location ):
			for aFile in files:
				if aFile.find( ".shp" ) < 0:
					continue
					
				try:
					LOG.debug( " ingestShapefiles %s " % ( os.path.join( root, aFile ) ) )
					objShapeReader	= shapefile.Reader( os.path.join( root, aFile ) )
			
					shapeRecs 		= objShapeReader.shapeRecords()
					
					LOG.debug( " ingestShapefiles %s shape record count is %0d" % ( os.path.join( root, aFile ), len( shapeRecs ) ) )
					
					for aShapeRec in shapeRecs:
						if not self.dictDatasets.has_key( aShapeRec.record[0] ):
							objDataset = MQDataSet( aShapeRec.record[0], 90.0, -180.0, -90.0, 180.0, 0, 100000000, {} )
							self.dictDatasets[ aShapeRec.record[0] ] = objDataset
						else:
							objDataset = self.dictDatasets[ aShapeRec.record[0] ]

						objDataset.importShapefileRecord( self.version_location, aShapeRec )
						
				except:
					LOG.debug( "Exception in importShapefile " + traceback.format_exc() )
					return False
    		
		return True					
					
    def process_coverage_data(self, xml_data, coord_base_path):
        ''' 
            Processes coverage datasets by iterating through the entries in the XML file.
            XML file is the main controller file that will be used to read the .vp files.
            Only files listed in the xml will be read during processing
        '''
        self.coverage = []
        for priority in xml_data.findall('priorityEntries/priorityEntry'):
            region_name = priority.get('validRegionName')
            data_set_name = ''
        
            # Valid region
            for valid_region in xml_data.findall('validRegions/validRegion'):
                if (0 == cmp(valid_region.get('name'), region_name)):
                    data_set_name = valid_region.get('dataSetName')
                    break
        
            # DataSet
            bnd_ul_lat = 0
            bnd_ul_lng = 0
            bnd_lr_lat = 0
            bnd_lr_lng = 0
        
            # Scale
            scale_lo = 0
            scale_hi = 0
            dictScale = {}
            
            for data_set in xml_data.findall('dataSets/dataSet'):
                
                if (0 == cmp(data_set.get('name'), data_set_name)):
                    ds_name = data_set.get('name')
                    lllist = data_set.findall('latLongBoundingRectangle')
                    #Bounding info
                    if (0 < len(lllist)):
                        for element in lllist:
                            bnd_ul_lat = float(element.get('ul_lat'))
                            bnd_ul_lng = float(element.get('ul_long'))
                            bnd_lr_lat = float(element.get('lr_lat'))
                            bnd_lr_lng = float(element.get('lr_long'))
                    else:
                        bnd_ul_lat = 0
                        bnd_ul_lng = 0
                        bnd_lr_lat = 0
                        bnd_lr_lng = 0
        
                    scalelist = data_set.findall('scaleRange')
                    # scale
                    if (0 < len(scalelist)):
                        for element in scalelist:
                            try:
                            	dictScale[ element.get('proj') ] = { "lo" : int(element.get('lo')), "hi" : int(element.get('hi')) }
                            	scale_lo = int(element.get('lo'))
                            	scale_hi = int(element.get('hi'))
                            except:
                            	scale_lo = int(element.get('lo'))
                            	scale_hi = int(element.get('hi'))
                    else:
                        scale_lo = 0
                        scale_hi = 0
                    break
            
            if (bnd_ul_lat == 0 and bnd_ul_lng == 0 and bnd_lr_lat == 0 and bnd_lr_lng == 0) :
                continue
#
#  Preferentially use the shapefile created dict of datasets as opposed to .vp files
#
            if self.dictDatasets.has_key( ds_name ):
                mq_data_set = self.dictDatasets[ ds_name ]
                LOG.debug( "Pulling ds %s info from shapefile dict, poly count is %s " % ( ds_name, len( mq_data_set.getPolygons() ) ) )
                mq_data_set.setBoundingBox( bnd_ul_lat, bnd_ul_lng, bnd_lr_lat, bnd_lr_lng )
                mq_data_set.setScales( scale_lo, scale_hi, dictScale )
            else:    
                LOG.debug( "Constructing ds %s info " % ( ds_name ) )
                mq_data_set = MQDataSet(ds_name, bnd_ul_lat, bnd_ul_lng, bnd_lr_lat, bnd_lr_lng, scale_lo, scale_hi, dictScale)
                mq_data_set.load_coord_set_list( coord_base_path )
                
            self.coverage.append( mq_data_set )   

    def getDataSetsByID( self, dsID ):
        ''' 
            Returns a list of datasets with the provided id
        '''
        return [ data_set for data_set in self.coverage if data_set.getID() == dsID ]

    def getDataSetsForScale( self, scale, projectionName ):
        ''' 
            Returns a list of all datasets valid for the provided scale
        '''
        return [ data_set for data_set in self.coverage if data_set.isCandidate( scale, projectionName ) == True ]

    #returns a list of dataset ids
    def getIntersectingDataSets( self, candidateDataSets, latlng_arr, bGetAllDataSets = False, bUsePolygon = False ):
        ''' 
			Returns a list of all datasets intersecting the provided geometry (copyright), or the first matching data set ID (coverage selection)
        '''
        arrDataSets		= []
            	
        if bUsePolygon and len( latlng_arr ) == 4:
            listPolygonRing	= [ [ point[1], point[0] ] for point in latlng_arr ]
            checkPolygon = Polygon( listPolygonRing )
		    
        for data_set in candidateDataSets:
#            LOG.debug("Checking polygons for ds %s, %d" % ( data_set.getName(), len( data_set.getPolygons() ) ) )
            for polygon in data_set.data_polygons:
                if bUsePolygon and len( latlng_arr ) == 4:
#                    LOG.debug("Checking a polygon for ds %s" % ( data_set.getName() ) )
#                    LOG.debug("polygon %s, check %s" % ( str( polygon ), str( checkPolygon ) ) )
                    if polygon.intersects( checkPolygon ) or polygon.within( checkPolygon ):
#                        LOG.debug("polygon intersects" )
                        if not bGetAllDataSets:
                            return [data_set.getID()]
                        elif data_set.getID() not in arrDataSets:
                            arrDataSets.append( data_set.getID() )
                else:
                    for latlng_pair in latlng_arr:
                        point = Point(latlng_pair[1], latlng_pair[0])  #  The underlying geometry is now x=lon; y=lat
						
                        if( polygon.contains( point ) ):
                            if not bGetAllDataSets:
                                return [data_set.getID()]
                            elif data_set.getID() not in arrDataSets:
                                arrDataSets.append( data_set.getID() )
			
        return arrDataSets

    def check_coverages(self, tile):
        ''' 
            Checks if all the lat, long present in the array input for that scale falls under the coverage available.
            Returns the dataset name that covers the given latlong at that scale
        '''
        
        if len( self.coverage ) <= 0:
            LOG.error('Coverage %s contains no datasets' % (self.version_location) )            
            return ''
            
        scale 				= tile.get_mapquest_scale()
        projectionName 		= tile.get_projection_name()
            
        candidateDataSets	= self.getDataSetsForScale( scale, projectionName )
        
        if len( candidateDataSets ) <= 0:
            LOG.error('Coverage %s contains no candidate datasets for scale %s, projection %s' % (self.version_location, str( scale ), projectionName ) )            
            return ''
            
        #cant use get bounding box since coverage check needs all four vertices.
        #tuple is immutable. so converting to list since value might get ressigned when lat/lng is out of range.
        lllatlng = list(tile.get_lat_lng(0, 0))
        urlatlng = list(tile.get_lat_lng(tile.get_tile_size(), tile.get_tile_size()))
        ullatlng = list(tile.get_lat_lng(0, tile.get_tile_size()))
        lrlatlng = list(tile.get_lat_lng(tile.get_tile_size(), 0))
        
        latlng_arr = [lllatlng, urlatlng, ullatlng, lrlatlng]

        if(lrlatlng[0] > 90.00 or ullatlng[0] < -90.00):
            raise ValueError("Invalid latitude lower latitude = " + str(lrlatlng[0]) + "; upper latitude = " + str(ullatlng[0]))
        
        LOG.debug('Checking coverage for scale :' + str(scale) + ' and for latlngArr :' + str(latlng_arr))
        
    	for my_pos in range(0, len(latlng_arr)):
            if(latlng_arr[my_pos][0] > 90):
                latlng_arr[my_pos][0] = 90
            elif(latlng_arr[my_pos][0] < -90):
                latlng_arr[my_pos][0] = -90
        #end foreach
        
        return self.getIntersectingDataSets( candidateDataSets, latlng_arr, False, False )
            
    def export_coverages_shapefiles( self ):
		for aDataSet in self.coverage:
			aDataSet.exportShapefile( self.version_location )
			
    #end checkCoverages
