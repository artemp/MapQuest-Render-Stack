#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  tile worker processing
#
#  Author: artem@mapnik-consulting.com
#  Author: david.lundeen@mapquest.com
#  Author: john.novak@mapquest.com
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

import zmq
import time
import random

from tile_pb2 import *

sys.path.append('./tile_worker')

from MQWatcher import *

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
    
MQWatcher()

context = zmq.Context()
socket = context.socket(zmq.REP)
socket.bind("tcp://*:8888")
 
while True:
	#  Wait for next request from client
	message = socket.recv()
	print "Received request: ", message
 
	#  Do some 'work'
	time.sleep( 1.0 / 5.0 ) 
 
	#  Send reply back to client
	messageProto 		= MetaTile()

	messageProto.z			= random.randrange( 0, 17 )
	messageProto.x			= random.randrange( 0, 2 ** messageProto.z )
	messageProto.y			= random.randrange( 0, 2 ** messageProto.z )
	messageProto.clientid	= 52
	messageProto.priority	= 100

	socket.send( messageProto.SerializeToString() )
