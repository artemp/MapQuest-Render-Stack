/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: artem@mapnik-consulting.com
 *
 *  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *-----------------------------------------------------------------------------*/

#ifndef SPHERICAL_MERCATOR_HPP
#define SPHERICAL_MERCATOR_HPP

#ifndef M_PI 
#define M_PI 3.141592653589793238462643
#endif

#define RAD_TO_DEG (180/M_PI)
#define DEG_TO_RAD (M_PI/180)

#include <cmath>

namespace rendermq
{

template <int levels=19>
class spherical_mercator
{
    double Ac[levels];
    double Bc[levels];
    double Cc[levels];
    double zc[levels];
    
public:
    spherical_mercator() 
    {    
        int d, c = 256;
        for (d=0; d<levels; d++) {
            int e = c/2;
            Bc[d] = c/360.0;
            Cc[d] = c/(2 * M_PI);
            zc[d] = e;
            Ac[d] = c;
            c *=2;
        }
    }
    
    void to_pixels(double &x, double &y, int zoom) 
    {
        double d = zc[zoom];
        double f = minmax(std::sin(DEG_TO_RAD * y),-0.9999,0.9999);
        x = round(d + x * Bc[zoom]);
        y = round(d + 0.5*std::log((1+f)/(1-f))*-Cc[zoom]);
    }
    
    void from_pixels(double &x, double &y, int zoom) 
    {
        double e = zc[zoom];
        double g = (y - e)/-Cc[zoom];
        x = (x - e)/Bc[zoom];
        y = RAD_TO_DEG * ( 2 * atan(exp(g)) - 0.5 * M_PI);
    }

private:
    double minmax(double a, double b, double c)
    {
#define LOCAL_MIN(x,y) ((x)<(y)?(x):(y))
#define LOCAL_MAX(x,y) ((x)>(y)?(x):(y))
        a = LOCAL_MAX(a,b);
        a = LOCAL_MIN(a,c);
        return a;
#undef LOCAL_MIN
#undef LOCAL_MAX
    }
};

}

#endif //SPHERICAL_MERCATOR_HPP
