''' Provides versioned coverage support    
'''
import logging
from coveragedata import Coverage
LOG = logging.getLogger(__name__)
 
class CoverageManager:
    '''
        Manager class to load, maintain coverage's for each of the version separately for satellite and map type. 
    '''
    # Dict to hold the coverage version location and dataset for map and sat
    coverage_datamap = {}

    def __init__(self):
        '''
            Initializing coverage manager. 
        '''
        LOG.info('Initializing coverage manager')
        
        #clear the map
        self.coverage_datamap.clear()

    def add_coverage(self, version_location, coverageKey=None ):
        '''
            Add coverage dataset for the given version
        '''
        try:
            LOG.debug('Creating coverage dataset from ' + version_location)
            coverage = Coverage(version_location)
            LOG.debug('Adding coverage dataset from ' + version_location)
            
            if coverageKey is not None:        
            	self.coverage_datamap[coverageKey] = coverage
            else:
            	self.coverage_datamap[version_location] = coverage
            return coverage
        except IOError, exp:
            LOG.error('Error occurred while loading coverage from ' + version_location + ' : %s' % exp)
            raise exp

    def get_coverage(self, version_location, addIfNotPresent, coverageKey = None):
        '''
            Get coverage dataset for given version location
        '''        
        if version_location in self.coverage_datamap:
            covdata = self.coverage_datamap[version_location]
            return covdata
        else:
            if addIfNotPresent:
                covdata = self.add_coverage(version_location, coverageKey)
                return covdata
            else:
            	LOG.debug( 'coverage dataset %s not found ' % ( version_location ) )
                raise ValueError('Coverage data not present in the coverage pool. Nothing to retrieve.' )

    def is_coverage_version_present(self, version_location):
        '''
            Return success when the coverage dataset for given version location is present in the coverage data map
            note : this method is not used as of now
        '''
        if version_location in self.coverage_datamap:
            return True
        else:
            return False

    def get_coverage_map(self):
        '''
            Get coverage dataset 
        '''
        return self.coverage_datamap

    def get_coverage_list(self):
        '''
            Return list of versioned coverages loaded 
        '''
        return self.coverage_datamap.keys()
    
    def load_coverage(self, version_location, reloadFlag, coverageKey = None ):
        '''
            Load/reload coverage dataset from the given version location
        '''
        if version_location not in self.coverage_datamap :
            #LOG.debug('Adding coverage dataset from '+ version_location)
            self.add_coverage(version_location, coverageKey)
        elif reloadFlag:
            LOG.debug('Reloading coverage dataset from ' + version_location)
            self.add_coverage(version_location, coverageKey)
        else:
            raise ValueError('Trying to add an already existing version. Data not added to the coverage pool from ' + version_location)
        
    def remove_coverage(self, version_location):
        '''
            Deletes coverage data from the coverage data map
        '''
        if version_location in self.coverage_datamap:
            del self.coverage_datamap[version_location]
            #TODO may need to delete from config also.
        else:
            raise ValueError('Coverage Data not present in the coverage pool. Nothing to remove.')

