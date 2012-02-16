#!/usr/bin/env python

import sys
import os
import errno
from ConfigParser import ConfigParser
from metatile import save_meta,xyz_to_meta,check_xyz
from metatile import METATILE
from watcher import Watcher
import dqueue
import tile_storage
import uuid
import csv
import time

#rendering related
from tile import Tile

def loadConfig(config_file):
    config = ConfigParser()
    config.read(config_file)

    try:
        storage_conf = dict(config.items('storage'))
        style_headers = csv.reader([config.get('worker', 'styles')],skipinitialspace=True).next()

    except Exception as ex:
        print>>sys.stderr,"ERROR: failed to load worker configuration from: %s (%s)" % (config, str(ex))
        sys.exit(1)

    formats = {}
    try:
        formats = dict(config.items('formats'))
	for name, format in formats.iteritems():
            formats[name] = csv.reader([format], skipinitialspace=True).next()

    except Exception as ex:
        print>>sys.stderr,"ERROR: failed to load worker configuration for formats from: %s (%s)" % (config, str(ex))

    styles = {}
    for style_header in style_headers :
        print>>sys.stderr,"Loading '%s' style" % style_header
        try :
            name = config.get(style_header, "type")
            styles[name] = formats[name]
        except Exception as ex:
            print>>sys.stderr,"ERROR: failed to load worker configuration for '%s' from: %s (%s)" % (style_header,config, str(ex))
            sys.exit(1)
    
    renames = {}
    for fmt in set([item for sublist in styles.values() for item in sublist]):
        try:
            opts = dict(config.items(fmt))
            if 'pil_name' in opts:
                renames[fmt] = opts['pil_name']
        except Exception as ex:
            print>>sys.stderr,"ERROR: failed to load worker configuration for format '%s' from: %s (%s)" % (fmt,config, str(ex))
            sys.exit(1)

    for style in styles:
        fmts2 = []
        for fmt in styles[style]:
            if fmt in renames:
                fmts2.append(renames[fmt])
            else:
                fmts2.append(fmt)
        styles[style] = fmts2

    return styles, tile_storage.TileStorage(storage_conf)

PROTO_FORMATS = { 
    'png': dqueue.ProtoFormat.fmtPNG,
    'jpg': dqueue.ProtoFormat.fmtJPEG,
    'jpeg': dqueue.ProtoFormat.fmtJPEG,
    'json': dqueue.ProtoFormat.fmtJSON,
    'gif': dqueue.ProtoFormat.fmtGIF
}

if __name__ == "__main__" :
    if len(sys.argv) != 4:
        print>>sys.stderr,"Usage: %s <worker-config-file-source> <worker-config-file-destinaton> <file-with-list-of-tiles>" % sys.argv[0]
        sys.exit(1)
     
    # load source and destination configs and set up storages
    src_styles, src_storage = loadConfig(sys.argv[1])
    dst_styles, dst_storage = loadConfig(sys.argv[2])

    # only copy styles which are in both configs.
    copy_styles = {}
    for style in list(set(src_styles.keys()) & set(dst_styles.keys())):
        formats = list(set(src_styles[style]) & set(dst_styles[style]))
        if len(formats) > 0:
            copy_styles[style] = formats

    # open the file with the list of tiles and iterate over
    # each line
    tiles = open(sys.argv[3], "r")
    line = tiles.readline()
    while line:
        z, x, y = [int(x) for x in line.split("/")]
        for style in copy_styles:
            tile = dqueue.TileProtocol()
            tile.x = (x & ~7)
            tile.y = (y & ~7)
            tile.z = z
            tile.style = style
            tile.format = dqueue.ProtoFormat(reduce(lambda a,b: a | b, [int(PROTO_FORMATS[f]) for f in copy_styles[style]]))
            tile.status = dqueue.ProtoCommand.cmdRender
            print "Would get %s" % str(tile)
            meta = src_storage.get_meta(tile)
            if meta is not None and len(meta) > 0:
                print "Got that meta file! yay!"
                success = dst_storage.put_meta(tile, meta)
                if success == False:
                    raise "Failed to put tile! Most bad error is."
        line = tiles.readline()

