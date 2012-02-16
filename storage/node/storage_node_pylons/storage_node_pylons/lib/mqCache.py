import os
import logging
import stat
import thread

log = logging.getLogger(__name__)

class mqCache():
    def __init__( self, root ):
        self.root = root
	
	""" Fetch a BLOB
    """
    def get(self, id):
    	if not self.is_available( id ):
    		return None
    		
    	try:
            hndFile = open( self.construct_name( id ), "rb" )
            bytes = hndFile.read()
            hndFile.close()
    	
    	except:
    		return None
    		
        return bytes
    
    """ Add/Update a BLOB
    """ 
    def put(self, id, data):
        try:
        	os.makedirs( self.construct_path( id ) )
        	
        except:
        	pass
              
        if not data:
            return None
            
        tmp_filename = self.root + ".tmp/"

        try:
            os.makedirs(tmp_filename)
        except:
            pass 

        # construct temporary file name using PID and thread ID so that 
        # concurrent requests can't overwrite each other. 
        tmp_filename += "." + str(os.getpid()) + "_" + str(thread.get_ident())

        """ For a atomic copy, we are using a temp file and then moving it using the os.rename
        """
        log.debug ("saving to " + tmp_filename)
        with open(tmp_filename, "wb") as fout:
            fout.write(data)
            fout.close()
        
        filename = self.construct_name( id )
        log.debug ("saving to " + filename )
        os.rename(tmp_filename, filename)

        return None
    
    """ Get location of a BLOB
    """
    def get_location(self, id):
        return self.construct_path( id )
    
    """ Check Availability
    """
    def is_available(self, id):
        return os.path.exists( self.construct_name( id ) )
    
    """ get modified time
    """
    def get_modified_time(self, id):
    	if self.is_available( id ):
    		return os.stat( self.construct_name( id ) )[stat.ST_MTIME]
    		
        return None
    
    """ set modified time
    """
    def set_modified_time(self, id, time):
    	if self.is_available( id ):
    		os.utime( self.construct_name( id ), ( time, time ) )
    		return True
    		
        return False
    
    def path_split(self, i):
        return [ "%03d" % x for x in [i / 1000000, (i / 1000) % 1000, i % 1000] ]

    def construct_path( self, id ):
        return os.path.join( self.root, id.replica, id.type, str( id.zoom ), *(self.path_split(id.x) + self.path_split(id.y)[:-1]))
        
    def construct_name( self, id ):
        return os.path.join( self.construct_path( id ), "%s.%s" % ( self.path_split(id.y)[-1], id.extension ) )
