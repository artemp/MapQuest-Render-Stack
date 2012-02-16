#!/usr/bin/python
#------------------------------------------------------------------------------
#
#  Spherical mercator calculations
#
#  Author: john.novak@mapquest.com
#
#  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
#
import math

class Mercator:
    DEG_TO_RAD = math.pi/180
    RAD_TO_DEG = 180/math.pi

    def __init__(self,levels=18):
        self.Bc = []
        self.Cc = []
        self.zc = []
        self.Ac = []
        c = 256
        for d in range(0,levels):
            e = c/2;
            self.Bc.append(c/360.0)
            self.Cc.append(c/(2 * math.pi))
            self.zc.append((e,e))
            self.Ac.append(c)
            c *= 2

    def minmax (self,a,b,c):
        a = max(a,b)
        a = min(a,c)
        return a
                
    def to_pixels(self,ll,zoom):
         d = self.zc[zoom]
         e = round(d[0] + ll[0] * self.Bc[zoom])
         f = self.minmax(math.sin(self.DEG_TO_RAD * ll[1]),-0.9999,0.9999)
         g = round(d[1] + 0.5*math.log((1+f)/(1-f))*-self.Cc[zoom])
         return (e,g)
    
    def from_pixels(self,px,zoom):
         e = self.zc[zoom]
         f = (px[0] - e[0])/self.Bc[zoom]
         g = (px[1] - e[1])/-self.Cc[zoom]
         h = self.RAD_TO_DEG * ( 2 * math.atan(math.exp(g)) - 0.5 * math.pi)
         return (f,h)

if __name__ == "__main__":
	objMercator = Mercator()
	
	thePixels = objMercator.to_pixels( [ -120.0, 36.0 ], 5 )	
	theLatLon = objMercator.from_pixels( thePixels, 5 )
	
	print thePixels, theLatLon