#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Coverage check renderer
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

# for NMS logging library
import mq_logging

# coverage checking object
from coverage.CoverageChecker import CoverageChecker

# for render result object
from renderResult import RenderResult

# for feature collection constructor
from geojson import FeatureCollection

class Renderer:
    def __init__( self, coverage_conf, coverages, factory ):
        # ensure that there are 'default' and 'missing; keys which 
        # are used when the coverage can't be looked up, or coverage
        # isn't found for a particular style.
        if 'default' not in coverages:
            raise Exception("Required config 'default' not found.")
        if 'missing' not in coverages:
            raise Exception("Required config 'missing' not found.")

        self.coverages = coverages
        self.factory = factory
        self.coverageChecker = CoverageChecker(coverage_conf)
        
    def process(self, tile):
        #get the list of coverages per tile, and the set of them removing duplicates
        coverages, uniqueCoverages = self.coverageChecker.checkSubTiles(tile, True)
        #how many different styles do these coverages map to?
        # note - have to lowercase and check the coverage names here, or they'll be
        # missed out or potentially cause a KeyError at runtime.
        sanitisedNames = []
        for coverage in uniqueCoverages:
            if coverage is not None and coverage.lower() in self.coverages:
                sanitisedNames.append(coverage.lower())
            else:
                sanitisedNames.append('default')
            
        mixedCoverage = len(set([self.coverages[coverage] for coverage in sanitisedNames])) > 1
        if mixedCoverage == True:
            mq_logging.info("Mixed coverage %s for style '%s' at metatile z=%s x=%s y=%s." % (uniqueCoverages, tile.style, tile.z, tile.x, tile.y))

        renderers = self._update_coverages(coverages, mixedCoverage, tile)

        if mixedCoverage is True:
            #TODO: do these in parallel, especially since the mapware one is done on a remote machine
            results = []
            for renderer in renderers:
                result = renderer.process(tile)
                if result is not None:
                    results.append(result)
                else:
                    results = []
                    break
            ret = self._combine(results, coverages) if len(results) > 0 else None
        else:
            ret = renderers[0].process(tile)

        if ret is None:
            raise "no image rendered for coverage(s) %s" % coverages
        else:
            return ret


    def _combine(self, results, coverages):
        # return one RenderResult combining all the results from the 
        # different coverages. since RenderResult is already split into
        # tiles, this means just gathering the tiles from the input.
        data = {}
        meta = {}

        for tileXY in coverages:
            index = coverages[tileXY]
            data[tileXY] = results[index].data[tileXY]
            if results[index].meta is None:
                meta[tileXY] = FeatureCollection([])
            else:
                meta[tileXY] = results[index].meta[tileXY]

        return RenderResult(data, meta)

    def _update_coverages(self, coverages, mixedCoverage, tile):
        renderer = []
        indices = {}
        #make sure all of the tiles have at least some rendering system
        for tileXY, coverage in coverages.iteritems():
            # did we have a coverage name. if not, then warn and
            # use the 'missing' one, which must exist as checked in the 
            # constructor.
            if len(coverage) > 0:
                # stupid config file results in names which are all in
                # lower case, so we have to follow that convention here.
                vend_name = coverage[0].lower()
            else:
                mq_logging.warning("No coverage for style '%s' at sub tile z=%s x=%s y=%s." % (tile.style, tile.z, tile.x + tileXY[0], tile.y + tileXY[1]))
                vend_name = 'missing'

            # check that the coverage name exists, or use the 'default'
            # keyed one, again checked in the constructor.
            if vend_name in self.coverages:
                style_name = self.coverages[vend_name]
            else:
                style_name = self.coverages['default']

            if style_name not in indices:
                indices[style_name] = len(renderer)
                    
                rend = self.factory.renderer_for(style_name)
                if rend is None:
                    raise Exception("Renderer for style name '%s' could not be retrieved." % style_name)

                renderer.append(rend)

            coverages[tileXY] = indices[style_name]

            #we are done if we dont need more than one renderer
            if mixedCoverage == False:
                break

        # return the list of renderers to use
        return renderer

