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

import time

#------------------------------------------------------------------------------
#------------------------------------------------------------------------------
class RenderTask:    
	def __init__( self, messageBuffer = None, dictAttributes = {} ):

		if messageBuffer != None:
			self.serializeFromBuffer( messageBuffer )
			
		if len( dictAttributes ) > 0:
			self.applyAttributes( dictAttributes )
			
	def serializeFromBuffer( self, messageBuffer ):		
		messageProto 	= MetaTile()
		messageProto.ParseFromString( messageBuffer )
		
		dictAttributes 					= {}
		dictAttributes[ 'gid' ]			= messageProto.gid 
		dictAttributes[ 'x' ]			= messageProto.x
		dictAttributes[ 'y' ]			= messageProto.y
		dictAttributes[ 'z' ]			= messageProto.z
		dictAttributes[ 'clientid' ]	= messageProto.clientid
		dictAttributes[ 'priority' ]	= messageProto.priority
		dictAttributes[ 'status' ]      = messageProto.status
		dictAttributes[ 'style' ]       = messageProto.style
		self.applyAttributes( dictAttributes )
			
	def serializeToBuffer( self, messageBuffer ):		
		messageProto 	= MetaTile()
		
		messageProto.gid				= self.gid
		messageProto.x					= self.x
		messageProto.y					= self.y
		messageProto.z					= self.z
		messageProto.clientid			= self.clientid
		messageProto.priority			= self.priority
		messageProto.status			= self.status
		messageProto.style                      = self.style
		
		return messageProto.ParseToString()
		
	def applyAttributes( self, dictAttributes ):
		self.gid		= dictAttributes[ 'gid' ]
		self.x			= dictAttributes[ 'x' ]
		self.y			= dictAttributes[ 'y' ]
		self.z			= dictAttributes[ 'z' ]
		self.clientID	= dictAttributes[ 'clientid' ]
		self.style = dictAttributes[ 'style' ]
		
		try:
			self.priority	= dictAttributes[ 'priority' ]
		
		except:
			self.priority	= -1

		try:
			self.status 	= dictAttributes [ 'status' ]
		except:
			self.status	= 0
		
	def getID( self ):
		return self.gid
	
	def getClientID (self):
		return self.clientID
	
	def getX( self ):
		return self.x
		
	def getY( self ):
		return self.y
		
	def getZ( self ):
		return self.z
		
	def getPriority( self ):
		return self.priority

	def getStatus  ( self ):
		return self.status

	def getStyle (self):
		return self.style

	def process( self ):
		print "Rendering Tile %d, %d, %d, %s" % ( self.z, self.x, self.y, self.style )
		time.sleep( 3 )
		pass

	def updateStatus(self, newStatus):
		self.status = newStatus

