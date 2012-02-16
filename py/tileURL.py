#!/usr/bin/env python
#------------------------------------------------------------------------------
#
#  Generate Tile Urls
#
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

import os
import errno
import sys
import getopt
import urllib2

#for generating tile urls
from tile import xyFromLatLng, METATILE
#for projection
from mercator import Mercator

def Usage():
	print>>sys.stderr,'Usage:'
        print>>sys.stderr,'-a=lat\t\tUpper left corner latitude for bounding box'
        print>>sys.stderr,'-b=lng\t\tUpper left corner longitude for bounding box'
        print>>sys.stderr,'-c=lat\t\tLower right corner latitude for bounding box'
        print>>sys.stderr,'-d=lng\t\tLower right corner longitude for bounding box'
        print>>sys.stderr,'-l=zoom\t\tLower zoom level bound'
        print>>sys.stderr,'-u=zoom\t\tUpper zoom level bound'
        print>>sys.stderr,'-p=prefix\t\tWill be prepended to url. Default is empty string'
        print>>sys.stderr,'-s=suffix\t\tWill be appended to url. Default is emtpy string'
        print>>sys.stderr,'-m\t\tGenerate a single tile URL per metatile only. Default generates all tile urls'
	print>>sys.stderr,'-q\t\tDo not show urls as they are generated'
	print>>sys.stderr,'-k\t\tStore the tiles on disk. For metatiles only the upper left tile will be stored'

#grab the value for a parameter that was passed in
def GetParam(optionName, options, inputs, default=None):
	try:
		value = inputs[options.index(optionName)]
		return value if value != '' else default
	except:
		return default

#do `mkdir -p`
def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as exc: # Python >2.5
        if exc.errno == errno.EEXIST:
            pass
        else: raise

#given a lat,lng bounding box and zoom get the the x, y range of tiles
def BoundingBoxToXYRange(ul, lr, zoom, snapToMeta = False, projection = Mercator(18+1)):
	x1, y1, z = xyFromLatLng(ul, zoom, projection)
	x2, y2, z = xyFromLatLng(lr, zoom, projection)
	#clamp to valid range
	x1 = max(x1, 0)
	y1 = max(y1, 0)
	#if we only want the upper left tile of the metatile
	if snapToMeta:
		x1 &= ~(METATILE - 1)
		y1 &= ~(METATILE - 1)
		x2 &= ~(METATILE - 1)
		y2 &= ~(METATILE - 1)
	#+1 because range is not inclusive
	last = 1 if z == 0 else int(2 ** z)
	x2 = min(x2 + 1, last)
	y2 = min(y2 + 1, last)
	return x1, y1, x2, y2

def GenerateZXY(zooms, ul, lr, meta=False, quiet=False):
	tiles = []
	#for each zoom level
        for zoom in zooms:
                #if we only want the upper left tile of each metatile
                if meta:
                        x1, y1, x2, y2 = BoundingBoxToXYRange(ul, lr, zoom, True)
                        stride = min(METATILE, 1 << zoom)
                else:
                        x1, y1, x2, y2 = BoundingBoxToXYRange(ul, lr, zoom, False)
                        stride = 1
                #print them out
                for x in range(x1, x2, stride):
                        for y in range(y1, y2, stride):
				tile = (zoom, x, y)
				if not quiet:
	                                print tile

				tiles.append(tile)
	return tiles

def GenerateURLs(zooms, ul, lr, prefix, suffix, meta=False, quiet=False, store=False):
	if store:
		opener = urllib2.build_opener()

	urls = []
	#for each zoom level
        for zoom in zooms:
                #if we only want the upper left tile of each metatile
                if meta:
                        x1, y1, x2, y2 = BoundingBoxToXYRange(ul, lr, zoom, True)
                        stride = min(METATILE, 1 << zoom)
                else:
                        x1, y1, x2, y2 = BoundingBoxToXYRange(ul, lr, zoom, False)
                        stride = 1
                #print them out
                for x in range(x1, x2, stride):
                        for y in range(y1, y2, stride):
				url = '%s%d/%d/%d%s' % (prefix, zoom, x, y, suffix)

				if store:
					error = True
					while error:
							try:
								#make the directory
								mkdir_p('./%d/%d' % (zoom, x))
								#load the page
								response = opener.open(url)
								#save the tile
								file = open('%d/%d/%d%s' % (zoom, x, y, suffix), 'w')
								file.write(response.read())
								#done with it
								response.close()
								file.close()
								error = False
							except Exception as detail:
								print>>sys.stderr,'%s' % (str(detail))
								print>>sys.stderr,'Retrying %s' % (url)

				if not quiet:
	                                print url

				urls.append(url)
	return urls

if __name__ == '__main__':

	try:
		#no long arguments, too lazy to implement for now...
		opts, args = getopt.getopt(sys.argv[1:], "a:b:c:d:l:u:p:s:mqk", [])
		options = [option[0] for option in opts]
		inputs = [input[1] for input in opts]
		required = set(['-a', '-b', '-c', '-d', '-l', '-u'])
		if required.intersection(options) != required:
			raise getopt.error('Usage', 'Missing required parameter')
	except getopt.GetoptError:
		Usage()
		sys.exit(2)	

	#get the args, +1 because range is not inclusive
	zooms = range(int(GetParam('-l',options,inputs)), int(GetParam('-u',options,inputs)) + 1)
	ul = (float(GetParam('-a',options,inputs)), float(GetParam('-b',options,inputs)))
	lr = (float(GetParam('-c',options,inputs)), float(GetParam('-d',options,inputs)))
	prefix = GetParam('-p',options,inputs,'')
	suffix = GetParam('-s',options,inputs,'')

	#generate the URLs
	GenerateURLs(zooms, ul, lr, prefix, suffix, '-m' in options, '-q' in options, '-k' in options)
