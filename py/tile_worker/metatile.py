#!/usr/bin/python
#------------------------------------------------------------------------------
#
#  tile worker processing
#
#  Author: john.novak@mapquest.com
#
#  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
#
import struct

def metatile_getSubtile( im, xOffset, yOffset, xSize, ySize ):
	view = im.view(xx * 256 , yy * 256, 256, 256)
	tile = view.tostring(format)
	return tile

class metatile:
	METATILE	= 8
	TILE_SIZE	= 256
	META_MAGIC 	= "META"

	def xyz_to_meta( self, tile_path, x,y, z):
		mask = self.METATILE -1
		x &= ~mask
		y &= ~mask
		hashes = {}
		
		for i in range(0,5):
			hashes[i] = ((x & 0x0f) << 4) | (y & 0x0f)
			x >>= 4
			y >>= 4
			
		meta = "%s/%d/%u/%u/%u/%u/%u.meta" % (tile_path, z, hashes[4], hashes[3], hashes[2], hashes[1], hashes[0])
		return meta

	def xyz_to_meta_offset( self, x,y,z):
		mask = self.METATILE -1
		offset = (x & mask) * self.METATILE + (y & mask)
		return offset
		
	def save_meta( self, tile_path,worker_id,x,y,z,size,im,format,subtiler=metatile_getSubtile):
		# Split image up into NxN grid of tile images
		tiles = {}
		for yy in range(0,size):
			for xx in range(0,size):
#				view = im.view(xx * 256 , yy * 256, 256, 256)
#				tile = view.tostring(format)
				tiles[(xx, yy)] = subtiler( im, xx * 256 , yy * 256, 256, 256 )
		meta_path = self.xyz_to_meta(tile_path,x,y,z)
		d = os.path.dirname(meta_path)
		if not os.path.exists(d):
			try:
				os.makedirs(d)
			except OSError:
				# Multiple threads can race when creating directories,
				# ignore exception if the directory now exists
				if not os.path.exists(d):
					raise
		tmp = "%s.tmp.%s" % (meta_path, worker_id)
		f = open(tmp, "w")
		f.write(struct.pack("4s4i", self.META_MAGIC, self.METATILE * self.METATILE, x, y, z))
		offset = len(self.META_MAGIC) + 4 * 4
		offset += (2 * 4) * (self.METATILE * self.METATILE)
		# Collect all the tile sizes
		sizes = {}
		offsets = {}
		for xx in range(0, size):
			for yy in range(0, size):
				mt = self.xyz_to_meta_offset(x+xx, y+yy, z)
				sizes[mt] = len(tiles[(xx, yy)])
				offsets[mt] = offset
				offset += sizes[mt]
		# Write out the offset/size table
		for mt in range(0, self.METATILE * self.METATILE):
			if mt in sizes:
				f.write(struct.pack("2i", offsets[mt], sizes[mt]))
			else:
				f.write(struct.pack("2i", 0, 0))
		# Write out the tiles
		for xx in range(0, size):
			for yy in range(0, size):
				f.write(tiles[(xx, yy)])
	
		f.close()
		os.rename(tmp, meta_path)
