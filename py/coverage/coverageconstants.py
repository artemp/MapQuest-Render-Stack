'''
    Constants used in coverage.
'''

COVERAGE_XML_FILENAME = 'mqmauto_t.xml'
COVERAGE_ARCHIVE_PATH = 'home_dir/coverage/archive/'
COVERAGE_BASE_PATH = 'home_dir/coverage/data/'

'''
    At present this file holds the HTTP response codes that will be returned during coverage check
    TODO Can be placed in some generic location as these are not specific to coverage
''' 
INVALID_INPUT_ERROR_CODE = 403
NODATA_ERROR_CODE = 404
INVALID_LATITUDE_ERROR_CODE = 405
TIMEOUT_ERROR = 408
DISK_ERROR = 502
SERVER_ERROR = 503
SOURCE_ERROR = 504
