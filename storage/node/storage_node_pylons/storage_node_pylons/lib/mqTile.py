
class mqTile:
	def __init__( self, type, zoom, x, y, extension, version, replica ):
		self.type		= type
		self.zoom		= zoom
		self.x			= x
		self.y			= y
		self.extension		= extension
		self.version		= version
		self.replica		= replica
		
	def __str__( self ):
		return str( self.type ) + ":" + str( self.zoom ) + ":" + str( self.x ) + ":" + str( self.y ) + ":" + str( self.extension ) + ":" + str( self.version ) + ":" + str( self.replica )
		
		
