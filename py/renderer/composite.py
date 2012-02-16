#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Composite renderer
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

# for image operations
import PIL
import PIL.Image
import PIL.ImageOps

#for NMS logging library
import mq_logging

#for tile dimensions
import tile as dims
 
# for render result object
from renderResult import RenderResult

# for feature collection constructor
from geojson import FeatureCollection

class Renderer:
   def __init__( self, layers, factory, background = None ):
       self.layers = layers
       self.factory = factory
       try:
           self.background = PIL.Image.new('RGBA', (dims.TILE_SIZE, dims.TILE_SIZE), tuple(map(int, background.split(','))))
       except:
           self.background = None

   def process(self, tile):
       results = []
       for layer in self.layers:
          tile.style = layer
          result = self.factory.renderer_for(layer).process(tile)
          if result is not None:
              results.append(result)
       
       if len(results) < 1:
           raise "no layers could be rendered"
       
       tileXYs = reduce(lambda a,b: a | b, [set(r.data.keys()) for r in results])

       data = {}
       meta = {}
       count = 0
       for tileXY in tileXYs:
           images = [r.data[tileXY] for r in results]
           metas = [r.meta[tileXY] for r in results if r.meta is not None]
           #add the background if it has one
           if self.background is not None:
               images.insert(0, self.background)
           data[tileXY] = reduce(Renderer.combineImage, images)
           if len(metas) > 0:
              meta[tileXY] = reduce(Renderer.combineMeta, metas)
           else:
              meta[tileXY] = FeatureCollection([])

       return RenderResult(data, meta)

   @staticmethod
   def combineImage(a, b):
      alpha = PIL.ImageOps.invert(b.split()[3])
      return PIL.Image.composite(a, b, alpha)

   @staticmethod
   def combineMeta(a, b):
      return FeatureCollection(a.features + b.features)

 
    
