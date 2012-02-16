'''Reads the .vp files and loads the datasets 
'''
import os
import logging
import traceback
import ConfigParser

from shapely.geometry import asPolygon
from shapely.geometry import Polygon
from shapely.geometry import asPoint
from shapely.geometry import Point

try:
	import shapefile
except:
	pass

copyrightIDs = { 'nt' : 'navteqCopyright.cfg', 
				'and' : 'andCopyright.cfg', 
				'osm' : 'osmCopyright.cfg', 
				'open_sat' : 'osmCopyright.cfg', 
				'nadb' : 'mapquestCopyright.cfg', 
				'mapquest' : 'mapquestCopyright.cfg', 
				'intermap' : 'intermapCopyright.cfg', 
				'i3' : 'icubedCopyright.cfg' }

LOG = logging.getLogger(__name__)

class MQDataSet:
    ''' 
        Class to load the coverage datasets 
    '''
    ds_name = ''
    bnd_rect_ul_lat = 0.0
    bnd_rect_ul_lng = 0.0
    bnd_rect_lr_lat = 0.0
    bnd_rect_lr_lng = 0.0
    coord_set_list = []
    scale_lo = 0.0
    scale_hi = 0.0
    
    dataset_attributes = {"copyright":"copyright_text", 
    						"copyrighthtml":"copyright_html", 
    						"copyrightshort":"copyright_text_short", 
    						"copyrighthtmlshort":"copyright_html_short", 
    						"copyrightid":"copyright_id", 
    						"copyrightgroup":"copyright_group", 
    						"copyrightinclude":"copyright_include", 
    						"featureid":"feature_id",
    						"vendorname":"vendor_name",
    						"polygoncount":"polygon_count",
    						"polygon":"polygon",
    						"datasetid":"dataset_id"}
    
    def __init__(self, tmp_ds_name, tmp_bnd_rect_ul_lat, tmp_bnd_rect_ul_lng,
                               tmp_bnd_rect_lr_lat, tmp_bnd_rect_lr_lng,
                               tmp_scale_lo, tmp_scale_hi, tmp_dictScale):
        ''' 
            Initialize the attributes required for checking coverage 
        '''
        self.ds_name = tmp_ds_name
        self.setBoundingBox( tmp_bnd_rect_ul_lat, tmp_bnd_rect_ul_lng, tmp_bnd_rect_lr_lat, tmp_bnd_rect_lr_lng )

        self.dictScale = {}
        self.setScales( tmp_scale_lo, tmp_scale_hi, tmp_dictScale )
        
        self.data_polygons = []
        self.dataset_id = ''
        self.vendor_name= ''
        self.coverage_name= None
        for external_copyright_attribute_name, copyright_attribute_name in self.dataset_attributes.iteritems ():
            setattr (self, copyright_attribute_name, None)
#
#  copyright_text is left as None or the handler check breaks
#
        self.copyright_text_short = ''
        self.copyright_html = ''
        self.copyright_html_short = ''
        self.copyright_include = ''

    def getName( self ):
    	return self.ds_name

    def getVendorName( self ):
	return self.vendor_name

    def getID( self ):
    	return self.dataset_id

    def getCoverageName( self ):
    	if self.coverage_name is not None:
    		return self.coverage_name
    	return self.vendor_name

    def getPolygons( self ):
    	return self.data_polygons
    	
    def setBoundingBox( self, tmp_bnd_rect_ul_lat, tmp_bnd_rect_ul_lng, tmp_bnd_rect_lr_lat, tmp_bnd_rect_lr_lng ):
        self.bnd_rect_ul_lat = tmp_bnd_rect_ul_lat
        self.bnd_rect_ul_lng = tmp_bnd_rect_ul_lng
        self.bnd_rect_lr_lat = tmp_bnd_rect_lr_lat
        self.bnd_rect_lr_lng = tmp_bnd_rect_lr_lng

    def setScales( self, tmp_scale_lo, tmp_scale_hi, tmp_dictScale ):
        self.scale_lo = tmp_scale_lo
        self.scale_hi = tmp_scale_hi        

        if tmp_dictScale is not None:
        	for aKey in tmp_dictScale.keys():
        		if aKey is not None:
        			self.dictScale[ aKey ] = tmp_dictScale[ aKey ]

    def isCandidate( self, scale, projectionName ):
        ''' 
            Is this dataset a candidate for tile intersection check based upon scale
        '''        
#
#  if no projection dict is present, or the key is not found, perform the test the old way
#
        if not self.dictScale.has_key( projectionName ): 
            if( scale > self.scale_hi or scale < self.scale_lo ):
                return False
                
            return True
#
#  Otherwise, test using projection specific scales
#
        dictProjScale = self.dictScale[ projectionName ]
                
        if( scale > dictProjScale[ "hi" ] or scale < dictProjScale[ "lo" ] ):
            return False

        return True
         
    def add_coord_set(self, coord_set):
        ''' 
            Modify data so that each coord pair is inside of quotes [33,44,55,66 ==> "33 44","55 66"]
            Add the list to cooord set 
        '''        
        points_tup = []
        point_parts = coord_set.split(",")
        for i in range (0, (len(point_parts) / 2)):
            x = float(point_parts[i * 2 + 1]) # store as lon/lat
            y = float(point_parts[i * 2])
            points_tup_part = (x, y)
            points_tup.append(points_tup_part)

        polygon = Polygon(points_tup)
        self.data_polygons.append(polygon)

    def load_config_include( self, coord_base_path, copyrightPath ):
        ''' 
            Opens, loads and processes copyright config files
            
			[MQCopyright]
			
			Copyright=Intermap
			CopyrightGroup=map
			CopyrightID=intermap
			FeatureID=map_na
			CopyrightHtml=http://blah  
         '''
         
        if len( copyrightPath ) <= 0:
        	return False
        	
        copyrightConfigPath	= os.path.join( coord_base_path, copyrightPath )
        
        try:           
            config = ConfigParser.ConfigParser()
            config.read( copyrightConfigPath )
            configItems	= config.items( "MQCopyright" )  
            
            for attrName, attrValue in configItems:          
                if MQDataSet.dataset_attributes.has_key( attrName ):
                	setattr( self, self.dataset_attributes[ attrName ], attrValue )
			
            return True
			
        except:
            LOG.error( 'Error processing Copyright Config File ' + str( copyrightConfigPath ) + '\n' + traceback.format_exc() )
            return False

    def dump(self):
        ''' 
            Lists the contents of dataset
        '''
        LOG.debug('ds_name       : ' + str(self.ds_name))
        LOG.debug('bnd_rect_ul_lat : ' + str(self.bnd_rect_ul_lat))
        LOG.debug('bnd_rect_ul_lng : ' + str(self.bnd_rect_ul_lng))
        LOG.debug('bnd_rect_lr_lat : ' + str(self.bnd_rect_lr_lat))
        LOG.debug('bnd_rect_lr_lng : ' + str(self.bnd_rect_lr_lng))
        LOG.debug('scale_lo         : ' + str(self.scale_lo))
        LOG.debug('scale_hi         : ' + str(self.scale_hi))
        LOG.debug('dictScale len    : ' + str(len(self.dictScale)))
        LOG.debug('dictScale        : ' + str(self.dictScale))
        
        LOG.debug('Copyright Text   : ' + str( self.copyright_text ))
        LOG.debug('Copyright HTML   : ' + str( self.copyright_html ))
        LOG.debug('Copyright ID     : ' + str( self.copyright_id ))
        LOG.debug('Copyright Group  : ' + str( self.copyright_group ))
        
        LOG.debug('Vendor Name      : ' + str( self.vendor_name ))
        LOG.debug('Coverage Name    : ' + str( self.coverage_name ))
        
        LOG.debug('Dataset ID       : ' + str( self.dataset_id ))
          
    def exportShapefile( self, coord_base_path ):
		try:
	  		objShapeWriter	= shapefile.Writer( shapefile.POLYGON )

			objShapeWriter.field( "DatasetNam", "S", "64" )
			objShapeWriter.field( "DatasetID", "N", "10", 0 )
			objShapeWriter.field( "MQScaleLo", "N", "10", 0 )
			objShapeWriter.field( "MQScaleHi", "N", "10", 0 )
			objShapeWriter.field( "SMScaleLo", "N", "10", 0 )
			objShapeWriter.field( "SMScaleHi", "N", "10", 0 )
			objShapeWriter.field( "CopyInclud", "S", "255" )
			objShapeWriter.field( "VendorName", "S", "64" )
			objShapeWriter.field( "Coverage", "S", "64" )
			
			for aPolygon in self.data_polygons:
				if True:
					listPoints = list( aPolygon.exterior.coords )
					listSwappedPoints = []
					
					for aPoint in listPoints:
						listSwappedPoints.append( [ aPoint[1], aPoint[0] ] )
						
					objShapeWriter.poly( parts=[ listSwappedPoints ] )
				else:
					objShapeWriter.poly( parts=[ list( aPolygon.exterior.coords ) ] )
				
				if len( self.dictScale ) > 0 and self.dictScale.has_key( "MERCATOR" ):
					objShapeWriter.record( self.ds_name, self.dataset_id, self.dictScale[ "MQ" ][ "lo" ], self.dictScale[ "MQ" ][ "hi" ], self.dictScale[ "MERCATOR" ][ "lo" ], self.dictScale[ "MERCATOR" ][ "hi" ], self.copyrightInclude, self.vendor_name, self.coverage_name )
				else:
					objShapeWriter.record( self.ds_name, self.dataset_id, self.scale_lo, self.scale_hi, self.scale_lo, self.scale_hi, self.copyrightInclude, self.vendor_name, self.coverage_name )
			
			objShapeWriter.save( coord_base_path + self.ds_name )  		
  		
		except:
			return False
    		
		return True
          
    def importShapefileRecord( self, coord_base_path, aShapeRec ):
		''' 
			Opens and ingests a .shp file record and any associated copyright info
		'''
		try:
 				polygon = Polygon( aShapeRec.shape.points )
				self.data_polygons.append( polygon )
	
				self.ds_name		= aShapeRec.record[0]
				self.dataset_id		= aShapeRec.record[1]
				
				self.dictScale[ "MQ" ] 						= {}
				
				self.dictScale[ "MQ" ][ "lo" ]				= int( aShapeRec.record[2] )
				self.dictScale[ "MQ" ][ "hi" ]				= int( aShapeRec.record[3] )
				
				self.dictScale[ "MERCATOR" ] 				= {}

				self.dictScale[ "MERCATOR" ][ "lo" ]		= int( aShapeRec.record[4] )
				self.dictScale[ "MERCATOR" ][ "hi" ]		= int( aShapeRec.record[5] )
  		
  				self.copyrightInclude						= aShapeRec.record[6]
  				self.vendor_name							= aShapeRec.record[7]
  				
  				if len( aShapeRec.record ) >= 9:
  					self.coverage_name						= aShapeRec.record[8]
  					
  				self.load_config_include( coord_base_path, self.copyrightInclude )
		except:
			LOG.debug( "importShapefileRecord for shape file %s failed" % ( os.path.join( coord_base_path, self.ds_name ) ) )
			return False
        	
		return True
        
    def importShapefile( self, coord_base_path ):
		''' 
			Opens and ingests a .shp file and ingests any associated copyright info
		'''
		try:
			objShapeReader	= shapefile.Reader( os.path.join( coord_base_path, self.ds_name ) )
	
			shapeRecs 		= objShapeReader.shapeRecords()
			
			LOG.debug( "importShapefile %s shape record count is %0d" % ( os.path.join( coord_base_path, self.ds_name ), len( shapeRecs ) ) )
			
			for aShapeRec in shapeRecs:
				self.importShapefileRecord( coord_base_path, aShapeRec )
  				
		except:
			LOG.debug( "Exception in importShapefile " + traceback.format_exc() )
			return False
    		
		return True
		
    def load_coord_set_list(self, coord_base_path):
        ''' 
            Opens a .shp or .vp file; preferring shp files
            Note the polygon count. Iteratively read the shape object and add to coord set list
        '''
        if self.importShapefile( coord_base_path ):
            return
			
        # Open this file for reading...
        file_to_process = coord_base_path + self.ds_name + '.vp'
            
        LOG.debug('load_coord_set_list opening file (' + file_to_process + ')' )
        
        try:
            coord_handle = open(file_to_process, "r")
        except IOError, exp:
            LOG.error('Cannot open file (' + file_to_process + '): %s' % exp)
            return
            
        LOG.debug('Processing [' + str(file_to_process) + ']')
        num_polys = 0
        num_poly_processed = 0
        # note : cant use for loop since lines need to be read even in the loop
        # looks like vp file are huge so not loading them completely into memory
        
        line = coord_handle.readline()
        while line:
            line = line.strip()
            pos = line.find('DatasetID=')
            if (pos != -1):
                line = line[pos + len('DatasetID='):]
                self.dataset_id = line.strip()
                break
            else:
                line = coord_handle.readline()
        while line:
            line 				= line.strip()
            
            attrList 			= line.split( "=", 1 )
            
            if len( attrList ) == 2:
				attrName			= attrList[0].lower()
				attrValue			= attrList[1]
				
				if MQDataSet.dataset_attributes.has_key( attrName ):
					setattr( self, self.dataset_attributes[ attrName ], attrValue )
					
            pos = line.find('PolygonCount=')
            if (pos == -1):
                if (0 < num_polys):
                    searchstr = 'Polygon.' + str(num_poly_processed) + '.Shape='
                    pos = line.find(searchstr)
                    if (pos != -1):
                        #LOG.debug(str( len( line )) + ' -- ' + str(len( searchstr) ) + ' ***** ' + str(line) + ' ***** ')
                        line = line.strip()
                        if(len(line) == len(searchstr)):
                            # process next line as poly set
                            line = coord_handle.readline() # TODO need to check if empty what if eof?
                            line = line.strip()
                            self.add_coord_set(line)
                            # inc counter(s)
                            num_poly_processed = num_poly_processed + 1
                        else:
                            # process rest of line as poly set
                            line = line[pos + len(searchstr):]
                            line = line.strip()
                            self.add_coord_set(line)
                            # inc counter(s)
                            num_poly_processed = num_poly_processed + 1

                    # Have we hit the limit yet?
                    if (num_polys == num_poly_processed):
                        break
            else:
                num_polys = int(line[13:])
            # read next line
            line = coord_handle.readline()
        # end while
        coord_handle.close()
# end class MQDataSet
