/*------------------------------------------------------------------------------
 *
 * For reading images.
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2011 Mapquest, Inc.  All Rights reserved.
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

#ifndef RENDERMQ_IMAGE_HPP
#define RENDERMQ_IMAGE_HPP

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/tuple/tuple.hpp>
#include <string>
#include "../tile_utils.hpp"

using boost::tuple;

namespace rendermq {

/* Simple image class, wrapping a private image implementation,
 * in this case, the GD library. This is the interface through
 * which to perform image loading, saving and compositing 
 * operations.
 */
class image : public boost::noncopyable {
public:
   virtual ~image();

   // accessor methods for the properties of the image.
   unsigned int width() const;
   unsigned int height() const;

   // merge an image on top of this one, optionally at an
   // offset (x, y) within the image.
   void merge(const boost::shared_ptr<image> &other, int x = 0, int y = 0);

   // serialise in the given format to an in-memory string.
   std::string save(protoFmt format, const boost::property_tree::ptree &config = boost::property_tree::ptree()) const;

   // factory method for creating images from in-memory image 
   // formats. NOTE: if the image creation fails, then the 
   // pointer returned will be null - so check it!
   static boost::shared_ptr<image> create(std::string &data, protoFmt fmt);

   // factory method for creating blank, transparent images.
   static boost::shared_ptr<image> create_transparent(unsigned int w, unsigned int h);

   // factory method for creating images from million dots
   // parameters. NOTE: if the image creation fails, then the
   // pointer returned will be null - so check it!
   static boost::shared_ptr<image> createMillionDotsImage(const std::vector<std::pair<int, int> >& pixelCoords, const int& width, const int& height,
      const tuple<int,int,int,int>& dotColor, const tuple<int,int,int,int>& outlineColor, const tuple<int,int,int,int>& backgroundColor,
      const int& dotSize, const int& outlineThickness);

   // create an image from a GD image (passed as a void pointer).
   // this method takes ownership of the GD pointer - do not call
   // gdImageDestroy yourself!
   static boost::shared_ptr<image> create_from_gd(void *gd_img);

protected:
   // hide library details from header file.
   struct pimpl;
   boost::scoped_ptr<pimpl> m_impl;

   // only constructable from the static image::create method.
   image(pimpl *);
};

} // rendermq namespace

#endif // RENDERMQ_IMAGE_HPP
