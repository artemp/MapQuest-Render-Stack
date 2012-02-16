#!/usr/bin/python
#------------------------------------------------------------------------------
#
#  Process tile worker arguments from command line
#
#  Author: john.novak@mapquest.com
#
#  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
#
import os
import sys
import getopt

class worker_opts:
	def __init__( self, args ):
		self.argv	= args
		
		self.brokeraddress	= None 
		self.worker_id		= "pid%08d" % os.getpid() 
		self.mapstyle		= None 
		self.tile_dir		= None
		self.timeouts		= 4
		
	#--------------------------------------------------------------------------
	def usage( self ):
		print "worker.py --address=192.168.0.0:8888 --mapstyle=thestyle --tiledir=/mnt/tiles --id=foo --timeouts=8"
		
	#--------------------------------------------------------------------------
	def process( self ):
		try:
			opts, args = getopt.getopt( self.argv, "a:d:hi:m:t:", [ "address=", "help", "id=", "mapstyle=", "tiledir=", "timeouts=" ])
			
		except getopt.GetoptError:
			self.usage()
			sys.exit()
			
		for opt, arg in opts:
			if opt in ("-h", "--help"):
				self.usage()
				sys.exit()
			elif opt in ("-a", "--address"):
				self.brokeraddress = arg
			elif opt in ("-t", "--tiledir"):
				self.tile_dir = arg
			elif opt in ("-i", "--id"):
				self.worker_id = arg
			elif opt in ("-m", "--mapstyle"):
				self.mapstyle = arg
			elif opt in ("-t", "--timeouts"):
				self.tile_dir = arg
	
	#--------------------------------------------------------------------------
	def validate( self ):
		if self.brokeraddress == None:
			return False
			
		if self.worker_id == None:
			return False
			
		if self.mapstyle == None: 
			return False
			
		if self.tile_dir == None:
			return False
			
		return True
		
	#--------------------------------------------------------------------------
	def getBrokerAddress( self ):
		return self.brokeraddress
		
	#--------------------------------------------------------------------------
	def getWorkerID( self ):
		return self.worker_id
		
	#--------------------------------------------------------------------------
	def getMapStyle( self ):
		return self.mapstyle
		
	#--------------------------------------------------------------------------
	def getTileDir( self ):
		return self.tile_dir
		
	#--------------------------------------------------------------------------
	def getTimeouts( self ):
		return self.timeouts

#--------------------------------------------------------------------------
#--------------------------------------------------------------------------
if __name__ == "__main__":
	objOpts = worker_opts( sys.argv[1:] )
	objOpts.process()