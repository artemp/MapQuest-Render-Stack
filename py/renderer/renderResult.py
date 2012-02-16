#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Object to hold the results of a render
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

from metacutter import cutFeatures

def cutImage(image, pixels, dimensions):
    cutImages = {}

    # invalid input
    if pixels[0] < 1 or pixels[1] < 1 or dimensions[0] < 1 or dimensions[1] < 1:
        return cutImages

    # figure out how big the sub tiles will be
    width = pixels[0] / dimensions[1]
    height = pixels[1] / dimensions[0]

    # invalid input
    if width < 1 or height < 1:
        return cutImages

    # get each sub tile into the dict
    for y in range(dimensions[0]):
        for x in range(dimensions[1]):
            cutImages[(y, x)] = image.crop((x * 256, y * 256, x * 256 + 256, y * 256 + 256))

    return cutImages

class RenderResult:
    def __init__(self, data, meta):
        self.data = data
        self.meta = meta

    @classmethod
    def from_image(cls, tile, data, meta=None):
        cut_data = cutImage(data, tile.size, tile.dimensions)
        if meta is None:
            cut_meta = None
        else:
            cut_meta = cutFeatures(meta, tile.size, tile.dimensions, False)

        obj = cls(cut_data, cut_meta)
        return obj

