#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Worker
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
import os
import errno
from ConfigParser import ConfigParser
from optparse import OptionParser
from metatile import make_meta,xyz_to_meta,check_xyz
from metatile import METATILE
from watcher import Watcher
from memory import get_virtual_size
import dqueue
import tile_storage
import uuid
import csv
import time
import mq_logging
import gc

#rendering related
from renderer.factory import RendererFactory
from tile import Tile
from PIL import Image
from mercator import Mercator
from coverage.CoverageChecker import CoverageChecker
from transcode import Transcode, TranscodeMeta
from geojson import dumps

def notify (job, queue):
    notified = False
    while notified == False:
        try:
            queue.notify(job)
            notified = True
        except RuntimeError, e:
            error_message = e.message
            if error_message.lower().find ("deadlock") == -1:
                raise
            else:
                mq_logging.error("deadlock error notify: %s" % error_message)
    
def loadConfig(config):
    try :
        storage_conf = dict(config.items('storage'))

    except Exception as ex:
        mq_logging.error("failed to load worker configuration from: %s (%s)" % (config, str(ex)))
        sys.exit(1)
    
    storage = tile_storage.TileStorage(storage_conf)
    formats = {}
    format_args = {}

    if config.has_option('worker','memory_limit_bytes'):
        mem_limit = int(config.get('worker','memory_limit_bytes'))
    else:
        mem_limit = None

    #load the coverages
    coverageChecker = CoverageChecker(dict(config.items('coverages')))

    #load the formats
    formats = dict(config.items('formats'))
    #parse out into a list
    for coverage, csvFormats in formats.iteritems():
        formats[coverage] = csv.reader([csvFormats], skipinitialspace=True).next()

    # load settings for each of the data formats
    for fmt in set(format for formatList in formats.values() for format in formatList):
        try:
            opts = dict(config.items(fmt))
            # nasty hack, since PIL seems unwilling to coerce types and 
            # configparser gives everything back as strings.
            if 'quality' in opts:
                opts['quality'] = int(opts['quality'])
            # again, nasty hack to allow the PIL name for a format to be
            # different from the name in the config file.
            if 'pil_name' not in opts:
                opts['pil_name'] = fmt
            format_args[fmt] = opts
        except Exception as ex:
            mq_logging.error("failed to load format configuration for '%s': %s" % (fmt, str(ex)))
            sys.exit(1)

    try:
        renderers = RendererFactory(config, formats, format_args, storage)

    except Exception as ex:
        mq_logging.error("failed to load renderer configuration from: %s (%s)" % (config, str(ex)))
        sys.exit(1)

    #hand them all back
    return storage, renderers, formats, format_args, coverageChecker, mem_limit

if __name__ == "__main__" :

    option_parser = OptionParser(usage="usage: %prog [options] <worker-config> <queue-config> [<worker_id>]")
    #option_parser.add_option("-h", "--help", dest="help", action="store_true", help="Print this helpful message.")
    option_parser.add_option("-l", "--logging-config", dest="logging_config",
                             help="Path to configuration file for logging.")

    (options, args) = option_parser.parse_args()

    if len(args) != 2 and len(args) != 3:
        mq_logging.error("Wrong number of command line arguments.")
        option_parser.print_help()
        sys.exit(1)
    
    Watcher()

    if options.logging_config:
        log_config = ConfigParser()
        mq_logging.configure_file(options.logging_config)

    config = ConfigParser()
    config.read(args[0])

    #load the items from the config
    storage, renderers, formats, format_args, coverageChecker, mem_limit = loadConfig(config)

    #use mercator projection
    projection = Mercator(18+1)

    # if worker ID provided then use it, else generate one.
    if len(args) == 3:
        worker_id = args[2]
    else:
        worker_id = str(uuid.uuid4())

    #so we can be on the look out for new jobs
    queue = dqueue.Supervisor(args[1], worker_id)

    #worker run loop
    job_counter = 0
    while True:
        try:
            job = queue.get_job()
        except RuntimeError, e:
            error_message = e.message
            if error_message.lower().find ("deadlock") == -1:
                raise
            else:
                mq_logging.error("deadlock error get_job: %s" % error_message)
                continue
        

        mq_logging.info("Got task: %s %s %s '%s' id=%d" % (job.z,job.x,job.y,job.style,job.id))
        
        try:
            img_formats = formats[job.style]
            #convert the job into a tile for the renderer
            tile = Tile(job, projection)
            # find out which renderer we are supposed to be using
            renderer = renderers.renderer_for(job.style)

            if renderer is None:
                raise Exception("Request for renderer `%s', which is not configured." % job.style)

        except Exception as ex:
            mq_logging.error("Couldn't fulfill request for z=%s x=%s y=%s style='%s', sending ignore. Error: %s." % (job.z, job.x, job.y, job.style, str(ex)))
            job.status = dqueue.ProtoCommand.cmdIgnore
            notify(job, queue)
            continue

        if not check_xyz(job.x,job.y,job.z):
            job.status = dqueue.ProtoCommand.cmdIgnore
            notify (job, queue)
        else:
            handle = storage.get(job)

            # try and figure out if we think the tile exists. note that here
            # the 'tile' is really the top left tile of a metatile, so it's
            # no guarantee the whole metatile exists.
            tile_exists = job.status!=dqueue.ProtoCommand.cmdDirty \
                and handle.exists() \
                and not handle.expired()

            # see if it's possible to get the whole metatile. if so, we can
            # send that back to the broker, but if not then it's possible the
            # metatile was only partially expired and has to be re-rendered.
            if tile_exists and job.status!=dqueue.ProtoCommand.cmdRenderBulk:
                data = storage.get_meta(job)
                if data is not None:
                    job.data = data
                else:
                    tile_exists = False
            
            # if we get to this point then there was a metatile and it has now
            # got data, so there's no point in rendering it again.
            if tile_exists:
                job.status = dqueue.ProtoCommand.cmdIgnore
                job.last_modified = handle.last_modified()
                mq_logging.info("EXISTS METATILE %d:%d:%d:%s tile-size=%d" % (job.z,job.x,job.y,job.style,len(job.data)))
                notify (job, queue)
            else:
                try:
                    result = renderer.process(tile)
                    if result is None:
                        raise "Worker: requested metatile could not be rendered"

                    imageFormats = [imageFormat for imageFormat in img_formats if (imageFormat != 'json')]
                    # transcode images from result into the various formats which are 
                    # defined for this style.
                    metaTile = Transcode(result, tile.dimensions[0], imageFormats, format_args)
                    #cut up features into tiles and from geojson featureCollections to strings
                    if 'json' in img_formats and result.meta is not None:
                        metaData = dict([(k, dumps(result.meta[k])) for k in result.meta]) 
                    else: 
                        metaData = None 
                    #save the tiles and the meta data to storage
                    meta_tile = make_meta(job, metaTile, metaData, imageFormats, tile.dimensions[0])
    
                    if job.status!=dqueue.ProtoCommand.cmdDirty and job.status!=dqueue.ProtoCommand.cmdRenderBulk :
                        job.data = meta_tile
                    mq_logging.info("DONE METATILE %d:%d:%d:%s tile-size=%d" % (job.z,job.x,job.y,job.style,len(job.data)))
                    job.status = dqueue.ProtoCommand.cmdDone
                    job.last_modified = int(time.time())
                except Exception as detail:
                        mq_logging.error('%s' % (detail))
			job.satus = dqueue.ProtoCommand.cmdIgnore

                notify (job, queue)

        handle = None

        # try to run a garbage collection to keep memory usage under
        # control. but this might be quite slow, so only run it 
        # every 10th job.
        job_counter += 1
        if (job_counter % 10) == 0:
            gc.collect()

        # check memory usage and, if a limit has been set and it has
        # been exceeded, then drop out of the while loop and shut 
        # down. this should cause monit to re-start the process.
        if mem_limit is not None:
            mem_size = get_virtual_size()
            if mem_size > mem_limit:
                mq_logging.warning("Memory size %d is more than memory limit %d, shutting down." % (mem_size, mem_limit))
                break
            

