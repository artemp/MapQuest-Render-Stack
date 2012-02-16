#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Composite renderer
#
#  Author: matt.amos@mapquest.com
#
#  Copyright 2011 Mapquest, Inc.  All Rights reserved.
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

# for NMS logging library
import mq_logging

# for tile protocol
import dqueue

# for render result object
from renderResult import RenderResult

# for transcoding stuff
from transcode import Transcode, TranscodeMeta
from metatile import save_meta,xyz_to_meta,check_xyz,METATILE,metatile_reader

FORMAT_LOOKUP = {
    "png256": dqueue.ProtoFormat.fmtPNG,
    "png":    dqueue.ProtoFormat.fmtPNG,
    "jpeg":   dqueue.ProtoFormat.fmtJPEG,
    "gif":    dqueue.ProtoFormat.fmtGIF,
    "json":   dqueue.ProtoFormat.fmtJSON
}

# this is intended for use as a facade over some other renderer
# which first checks storage and, if there's no result in storage,
# calls to the other renderer and saves the result to storage.
class Renderer:
   def __init__( self, storage, formats, format_args, backend_renderer ):
       self.storage = storage
       self.formats = formats
       self.format_args = format_args
       self.backend_renderer = backend_renderer

   def job_formats(self, fmts):
       xfmt = 0
       for fmt in fmts:
           xfmt = xfmt | int(FORMAT_LOOKUP[fmt])
       return dqueue.ProtoFormat(xfmt)

   def process(self, tile):
       job = dqueue.TileProtocol()
       job.x = tile.x
       job.y = tile.y
       job.z = tile.z
       job.style = tile.style
       job.format = self.job_formats(self.formats)
       data = self.storage.get_meta(job)
       
       if data is None:
           # read only style
           if self.backend_renderer is None:
               return None

           # tile was not in storage - delegate to renderer
           result = self.backend_renderer.process(tile)

           # save tile to storage if result is good
           imageFormats = [imageFormat for imageFormat in self.formats if (imageFormat != 'json')]
           
           # transcode images from result into the various formats which are 
           # defined for this style.
           metaTile = Transcode(result, tile.dimensions[0], imageFormats, self.format_args)

           #cut up features into tiles and from geojson featureCollections to strings
           if 'json' in self.formats and result.meta is not None:
               metaData = dict([(k, dumps(result.meta[k])) for k in result.meta]) 
           else: 
               metaData = None 
                
           #save the tiles and the meta data to storage
           save_meta(self.storage, job, metaTile, metaData, imageFormats, tile.dimensions[0])
           
       else:
           # got result from storage, now need to unpack and return
           reader = metatile_reader(data)
           img = reader.image()
           meta = reader.metadata()
           result = RenderResult(img, meta)
           
       # pass through the original result
       return result
