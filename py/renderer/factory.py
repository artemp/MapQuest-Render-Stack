#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Renderers package
#
#  Author: matt.amos@mapquest.com
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

# import all the renderers
import mapnik
import terrain
import aerial
import composite
import coverages
from storage import Renderer as StorageRenderer

# other modules needed
import mq_logging
import csv
import sys
from collections import defaultdict

class RendererFactory:
    def __init__(self, config, formats, format_args, storage):
        self.renderers = {}
        
        # the config file gives the names of all the styles, separated by 
        # commas, in the [worker] section. each of these styles is then 
        # expected to be a section name, naming the style. this is what 
        # the job is matched against when it comes in. one style name is
        # one renderer.
        style_names = csv.reader([config.get('worker', 'styles')],skipinitialspace=True).next()
        # the styles which will be saved to storage when they are done.
        # note that it's possible for stuff to not be saved to storage
        # and this might be reasonable when the work involved in making
        # that tile is very low.
        save_names = csv.reader([config.get('worker', 'saved_styles')],skipinitialspace=True).next()
	
        # read only styles
        try:
            read_only_names = csv.reader([config.get('worker', 'read_only_styles')],skipinitialspace=True).next()
        except:
            read_only_names = []

        for style_name in style_names:
            mq_logging.info("Loading '%s' style" % style_name)
            try :
                # sanity check - there's no real problem with styles being
                # defined twice, but it probably indicates a typo or other
                # error in the config file.
                if style_name in self.renderers:
                    raise Exception("style '%s' is defined twice." % style_name)

                # if the style is only to be read from storage
                if style_name in read_only_names:
                    renderer = StorageRenderer(storage, formats[style_name], format_args, None)
                else:
                    renderer = self._create(config, style_name, storage, formats, format_args)

                # if the style name is in the formats config then it's supposed
                # to be saved to storage. so wrap the renderer in a saving
                # renderer.
                if (renderer is not None) and (style_name in save_names):
                    renderer = StorageRenderer(storage, formats[style_name], format_args, renderer)

                if renderer is None:
                    raise Exception("unable to create renderer from style '%s'." % style_name)

                self.renderers[style_name] = renderer

            except Exception as ex:
                mq_logging.error("failed to load worker configuration for '%s' from: %s (%s)" % (style_name,config, str(ex)))
                sys.exit(1)

    # factory method to create renderers. note that this is quite messy
    # and needs to know about all the renderers. this would be cleaner if
    # it were moved to the renderer classes themselves, perhaps as a static
    # method / alternate constructor.
    def _create(self, config, style_name, storage, formats, format_args):
        renderer = None
        rendering_system = config.get(style_name, "system")

        if rendering_system == 'mapnik':
            default_style = config.get(style_name, "default_style")
            mask_style = config.get(style_name, "mask_style") if config.has_option(style_name, "mask_style") else None
            renderer = mapnik.Renderer(default_style, mask_style)
       	    for (polygon, style) in config.items(style_name):
               	# teh uglies. to be refactored. and not only because the keys are downcased...
                if polygon != "default_style" and polygon != "mask_style" and polygon != "system":
       	            renderer.add_region(polygon, style, polygon)

        elif rendering_system == 'terrain':
            renderer = terrain.Renderer(config.get(style_name, "tr_config"))

        elif rendering_system == 'aerial':
            renderer = aerial.Renderer(config.get(style_name, "ae_config"))

        elif rendering_system == 'composite':
            layers = config.get(style_name, 'layers')
            layers = csv.reader([layers], skipinitialspace=True).next()
            background = config.get(style_name, 'background') if 'background' in dict(config.items(style_name)) else None
            renderer = composite.Renderer(layers, self, background)

        elif rendering_system == 'coverages':
            coverage_conf = dict(config.items('coverages'))
            vendor_conf = dict(config.items(style_name))
            del vendor_conf['system']
            renderer = coverages.Renderer(coverage_conf, vendor_conf, self)

        else:
            mq_logging.error("'%s' is not a known rendering system." % rendering_system)

        return renderer

    def renderer_for(self, style_name):
        if style_name in self.renderers:
            return self.renderers[style_name]
        else:
            return None

