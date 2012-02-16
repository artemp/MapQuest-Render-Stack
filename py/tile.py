#for tile conversion to ll bbox
import math
#for making sub tiles from master tile
from copy import copy

#constants
METATILE = 8
TILE_SIZE = 256
DPI = 72
scales = [443744033, 221872016, 110936008, 55468004, 27734002, 13867001, 6933501, 3466750, 1733375, 866688, 433344, 216672, 108336, 54168, 27084, 13542, 6771, 3385, 1693, 846, 423]


#bring within a circular range
def CircularClamp(low, high, value):
	#this hasn't been tested thoroughly...

	#swap
	if low > high:
		high, low = low, high
	#get the length of the range
	length = (high - low) + 1
	#if we need to move down
	if value < low:
		#move the range window closer to the actual value
		moved = (int(abs(value - low) / length) * -length) + low
		#how far off from the end of the range
		return high - abs(value - moved)
	#if we need to move up
	elif value > high:
		#move the range window closer to the actual value
		moved = (int(abs(value - high) / length) * length) + high
		#how far off from the end of the range
		return low + abs(value - moved)
	#if we are in range already
	else:
		return value

#return an x, y, z given lat, lng, zoom and projection
def xyFromLatLng(latLng, zoom, projection):
	#clamp the ll in bounds
	lat = CircularClamp(-90.0, 90.0, latLng[0])
	lng = CircularClamp(-180.0, 180.0, latLng[1])
	#project it to pixel space
	pixels = projection.to_pixels((lng,lat), zoom)
	#get x and y
	x = int(pixels[0] / TILE_SIZE)
	y = int(pixels[1] / TILE_SIZE)
	#hand them back
	return x, y, zoom

#returns bounding box, center projected ll, and pixel coords given x y z and dimension of metatile
def getBoundingBox(x, y, z, dimension, projection):
	#pixel location of the center of the metatile
	x0 = x * TILE_SIZE
	y0 = (y + dimension) * TILE_SIZE
	x1 = (x + dimension) * TILE_SIZE
	y1 = y * TILE_SIZE
	#from pixels to WGS 84, comes back as (lng, lat)
	ul = projection.from_pixels((x0, y0), z)
	lr = projection.from_pixels((x1, y1), z)
	#using ldexp for performant floating point division by 2 to get center pixel location
	center = (int(math.ldexp(x0 + x1, -1) + .5) , int(math.ldexp(y0 + y1, -1) + .5))
	center = projection.from_pixels(center, z)
	return ((ul[1], ul[0]), (lr[1], lr[0])), (center[1], center[0])

#calculate the scale on the fly, ignore y scale
def calculateScale(tile):
	EARTH_RADIUS_INCHES = 3963.190 * 5280 * 12
	PIX_PER_EARTH_RADIUS = DPI * EARTH_RADIUS_INCHES
	DEGREE_2_RADIAN = math.pi / 180
	lng = tile.bbox[1][1] - tile.center[1]
	while (lng < -180.000001):
		lng += 360.0
	while (lng > 180.000001):
		lng -= 360.0
	return int(((DEGREE_2_RADIAN * lng * 2 * PIX_PER_EARTH_RADIUS) / tile.size[0]) + .5)

#mapware tile structure
class Tile:

	#constructor
	def __init__(self, job, projection):
		#the number of rows/columns of sub tiles in the metatile
		size = min(METATILE, 1 << job.z)
		#get the x, y of the metatile instead of the sub tile
		self.x = job.x & ~(METATILE - 1)
		self.y = job.y & ~(METATILE - 1)
		self.z = job.z
		self.projection = projection
		#get the bounding box and center ll
		self.bbox, self.center = getBoundingBox(self.x, self.y, self.z, size, self.projection)
		#self.x = job.x
		#self.y = job.y
		#save pixel dimensions
		pixels = size * TILE_SIZE
		#save different bits of info
		self.size = (pixels, pixels)
		self.style = job.style
		self.scale = scales[job.z]
		self.dimensions = (size, size)
		#place holder for mapware image type
		self.format = None

	#returns a subtile of this meta tile
	def getSubTile(self, row, column):
		#if its a valid subtile of this metatile
		if column < self.dimensions[1] and row < self.dimensions[0]:
			#make a new tile
			tile = copy(self)
			tile.dimensions = (1, 1)
			tile.x = self.x + column
			tile.y = self.y + row
			tile.size = (int((self.size[0] * (1.0 / self.dimensions[1])) + .5), int((self.size[1] * (1.0 / self.dimensions[0])) + .5))
			#get this tiles pixel coords from the metatile
			tile.bbox, tile.center = getBoundingBox(tile.x, tile.y, tile.z, 1, tile.projection)
			return tile
		#this wasn't a valid subtile
		else:
			return None;

	def __repr__(self):
		return '<Tile style:%s size:%s scale:%s center:%s bbox:%s x:%s y:%s z:%s dimensions:%s projection:%s format:%s>' % (str(self.style), str(self.size), str(self.scale), str(self.center), str(self.bbox), str(self.x), str(self.y), str(self.z), str(self.dimensions), str(self.projection), str(self.format))
