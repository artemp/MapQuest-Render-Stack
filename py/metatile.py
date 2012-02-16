import os
import sys
import struct
import dqueue
import PIL
from geojson import loads
from StringIO import StringIO

import mq_logging
import tile as dims

METATILE = dims.METATILE
META_MAGIC = "META"
# NOTE: this is a sanity check value. the per-style max zoom
# checks are done in the handler.
MAX_ZOOM=30

FORMAT_LOOKUP = {
    "png256": dqueue.ProtoFormat.fmtPNG,
    "png":    dqueue.ProtoFormat.fmtPNG,
    "jpeg":   dqueue.ProtoFormat.fmtJPEG,
    "gif":    dqueue.ProtoFormat.fmtGIF,
    "json":   dqueue.ProtoFormat.fmtJSON
}

FORMAT_REVERSE = {
    dqueue.ProtoFormat.fmtPNG  : "png",
    dqueue.ProtoFormat.fmtJPEG : "jpeg",
    dqueue.ProtoFormat.fmtGIF  : "gif",
    dqueue.ProtoFormat.fmtJSON : "json"
}

def xyz_to_meta(tile_path, x,y, z, style):
    mask = METATILE -1
    x &= ~mask
    y &= ~mask
    hashes = {}
    
    for i in range(0,5):
        hashes[i] = ((x & 0x0f) << 4) | (y & 0x0f)
        x >>= 4
        y >>= 4
        
    meta = "%s/%s/%d/%u/%u/%u/%u/%u.meta" % (tile_path, style, z, hashes[4], hashes[3], hashes[2], hashes[1], hashes[0])
    return meta

def xyz_to_meta_offset(x,y,z):
    mask = METATILE -1
    offset = (y & mask) * METATILE + (x & mask)
    return offset



class metatile_builder:
    def __init__(self, tile, size):
        self.meta_tile = ""
        self.tile = tile
        self.size = size
        self.offset = 0

    def write_offsets(self, contents):
        sizes = {}
        offsets = {}
        #rows
        for y in range(0, self.size):
            #columns
            for x in range(0, self.size):
                mt = xyz_to_meta_offset(self.tile.x + x, self.tile.y + y, self.tile.z)
                #elements accessed in row column order
                sizes[mt] = len(contents[(y, x)])
                offsets[mt] = self.offset
                self.offset += sizes[mt]
        # Write out the offset/size table
        for mt in range(0, METATILE * METATILE):
            if mt in sizes:
                self.meta_tile += struct.pack("2i", offsets[mt], sizes[mt])
            else:
                self.meta_tile += struct.pack("2i", 0, 0)

    def write_contents(self, contents):
        #rows
        for y in range(0, self.size):
            #columns
            for x in range(0, self.size):
                #elements accessed in row column order
                self.meta_tile += contents[(y, x)]
                contents[(y, x)] = None
    
    def write_header(self, format):
        format_as_int = FORMAT_LOOKUP[format]
        self.meta_tile += struct.pack("4s5i", META_MAGIC, METATILE * METATILE, self.tile.x, self.tile.y, self.tile.z, format_as_int)

    def offset_header(self):
        self.offset += len(META_MAGIC) + 5 * 4
        self.offset += (2 * 4) * (METATILE * METATILE)

def make_meta(job, tiles, metaData, formats, size):
    # Make some room for the headers and offsets
    builder = metatile_builder(job, size)
    for f in formats:
        builder.offset_header()
    if metaData is not None:
        builder.offset_header()

    # Collect all the tile sizes
    for f in formats:
        builder.write_header(f)
        builder.write_offsets(tiles[f])
    if metaData is not None:
        builder.write_header('json')
        builder.write_offsets(metaData)

    # Write out the tiles
    for f in formats:
        builder.write_contents(tiles[f])
    if metaData is not None:
        builder.write_contents(metaData)

    return builder.meta_tile

def save_meta(storage, job, tiles, metaData, formats, size):
    meta = make_meta(job, tiles, metaData, formats, size)
    # Send the meta tile to storage
    success = storage.put_meta(job, meta)
    # Make sure we know about it if the storage doesn't work. The cluster
    # will continue working, as the data can be sent back via the broker,
    # but it's helpful to have something in the logs so we know what's 
    # going on...
    if not success:
        mq_logging.error("Failed to save meta tile to storage (%d:%d:%d:%s tile-size=%d)" % \
                             (job.z,job.x,job.y,job.style,len(job.data)))

    # Return the meta tile
    return meta

def check_xyz(x,y,z):
    bad_coords = ( z < 0 or z > MAX_ZOOM)
    if not bad_coords:
        max_xy = (1 << z) -1
        bad_coords = (x < 0 or x > max_xy or y < 0 or y > max_xy)
    return not bad_coords
        
class metatile_reader:
    class tileset:
        def __init__(self, x, y, z, fmt, tiles):
            self.x = x
            self.y = y
            self.z = z
            self.fmt = fmt
            self.tiles = tiles

        def unImage(self):
            if len(self.tiles) != METATILE * METATILE:
                raise Exception("Unexpected metatile size, %d. Expected %d." % (len(self.tiles), METATILE * METATILE))
            img = {}
            for i in range(0, METATILE * METATILE):
                x = i % 8
                y = i / 8
                # not all tiles are necessarily present in a metatile
                if len(self.tiles[i]) > 0:
                    #referenced in row column order
                    img[(y, x)] = PIL.Image.open(StringIO(self.tiles[i])).convert('RGBA')
            return img

        def unJSON(self):
            if len(self.tiles) != METATILE * METATILE:
                raise Exception("Unexpected metatile size, %d. Expected %d." % (len(self.tiles), METATILE * METATILE))
            json = {}
            for i in range(0, METATILE * METATILE):
                x = i % 8
                y = i / 8
                # not all tiles are necessarily present in a metatile
                if len(self.tiles[i]) > 0:
                    #referenced in row column order
                    json[(y, x)] = loads(self.tiles[i])
            return json

    def __init__(self, data):
        self.tiles = []
        offset = 0
        header_str = struct.Struct("4s5i")
        offset_str = struct.Struct("2i")
        while offset + header_str.size < len(data):
            magic, n_tiles, tile_x, tile_y, tile_z, fmt_int = header_str.unpack_from(data, offset)
            if magic != META_MAGIC:
                break
            offset += header_str.size
            if offset + n_tiles * offset_str.size >= len(data):
                break
            tiles = []
            for i in range(0, n_tiles):
                off, sz = offset_str.unpack_from(data, offset)
                offset += offset_str.size
                tiles.append(buffer(data, off, sz))
            self.tiles.append(self.tileset(tile_x, tile_y, tile_z, FORMAT_REVERSE[fmt_int], tiles))

    def image(self):
        for i, t in enumerate(self.tiles):
            if t.fmt != "json":
                return t.unImage()
        return None

    def metadata(self):
        for i, t in enumerate(self.tiles):
            if t.fmt == "json":
                return t.unJSON()
        return None
