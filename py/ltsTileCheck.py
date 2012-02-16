#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Checks if tiles are in storage
#
#  Author: matt.amos@mapquest.com
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
sys.path.append( "../" )
from ConfigParser import ConfigParser
import tile_storage
import fileinput
import dqueue
import re
import getopt


FORMAT_LOOKUP = {
	"png256": dqueue.ProtoFormat.fmtPNG,
	"png":    dqueue.ProtoFormat.fmtPNG,
	"jpeg":   dqueue.ProtoFormat.fmtJPEG,
	"jpg":   dqueue.ProtoFormat.fmtJPEG,
	"gif":    dqueue.ProtoFormat.fmtGIF,
	"json":   dqueue.ProtoFormat.fmtJSON
}

#load the storage configuration from file
def load(configFile):
	config = ConfigParser()
	config.read(configFile)
	storage_conf = dict(config.items('storage'))
	storage = tile_storage.TileStorage(storage_conf)
	return storage

def Usage():
        print>>sys.stderr,'Usage: ./lts.py config tiles.txt'
        print>>sys.stderr,'-c=config\t\tStorage config'
        print>>sys.stderr,'-f=file\t\t\tFile of urls'
	print>>sys.stderr,'-v=freq\t\t\tVerbose mode print status after freq lines processed'

#grab the value for a parameter that was passed in
def GetParam(optionName, options, inputs, default=None):
        try:
                value = inputs[options.index(optionName)]
                return value if value != '' else default
        except:
                return default

if __name__ == '__main__':

        try:
                #no long arguments, too lazy to implement for now...
                opts, args = getopt.getopt(sys.argv[1:], "c:f:v:", [])
                options = [option[0] for option in opts]
                inputs = [input[1] for input in opts]
                required = set(['-c', '-f'])
                if required.intersection(options) != required:
                        raise getopt.error('Usage', 'Missing required parameter')
        except getopt.GetoptError:
                Usage()
                sys.exit(2)

	#get the storage object
	storage = load(GetParam('-c', options, inputs))

	#get the verbosity
	try:
		verbosity = int(GetParam('-v', options, inputs))
	except:
		verbosity = None

	#for each line
	count = 0
	for line in fileinput.input(GetParam('-f',options,inputs), inplace = 0):
		line = line.lstrip().rstrip()
		if verbosity is not None and count % verbosity == 0:
			print 'finished %d lines' % count
		count = count + 1
		#get the style z x y format
		parts = re.split(r'[/\.]',line)
		parts = [x for x in parts if len(x) > 0]
		if len(parts) != 5:
			continue
		#make the job and ask for it
		job = dqueue.TileProtocol()
		try:
			job.x = int(parts[2])
			job.y = int(parts[3])
			job.z = int(parts[1])
		except:
			continue
		job.style = parts[0]
		if FORMAT_LOOKUP.has_key(parts[4]) == False:
			continue
		job.format = FORMAT_LOOKUP[parts[4]]
		#only care about single tile in the metatile
		handle = storage.get(job)
		if handle.exists() == False:
			print line


	
