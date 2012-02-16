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

#include "image.hpp"
#include "../logging/logger.hpp"
#include <gd.h>
#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/foreach.hpp>
#include <set>
#include <map>

// the quality setting for JPEG writing if none is specified in
// the config file.
#define DEFAULT_JPEG_QUALITY (80)

using std::string;
using std::set;
using std::map;
using boost::shared_ptr;
using boost::optional;
namespace bt = boost::property_tree;

namespace 
{ 
/* method which attempts to palettize img_ptr by counting the
 * number of distinct colours in-use in *img_ptr. if that's
 * less than 256 it replaces the image pointed-at by the new
 * palettized image and destroys the old one.
 */
void try_image_palettize(gdImagePtr *img_ptr)
{
   gdImagePtr img = *img_ptr;

   if (gdImageTrueColor(img) != 0)
   {
      const int sx = img->sx;
      const int sy = img->sy;
      set<int> colours_used;

      // count the unique number of colours used.
      for (int y = 0; y < sy; ++y)
      {
         for (int x = 0; x < sx; ++x)
         {
            colours_used.insert(gdImageTrueColorPixel(img, x, y));
         }
         if (colours_used.size() > 255)
         {
            return;
         }
      }

      // can fit colours into 8-bit packed palette - otherwise we
      // would have exited the loop above.
      gdImagePtr pal_img = gdImageCreatePalette(sx, sy);
      map<int, int> palette;
      BOOST_FOREACH(int c, colours_used)
      {
         // allocate an entry for each new colour and keep a 
         // mapping record so that we can convert the pixel
         // values directly later.
         palette[c] = gdImageColorAllocateAlpha(
            pal_img, 
            gdTrueColorGetRed(c),
            gdTrueColorGetGreen(c),
            gdTrueColorGetBlue(c),
            gdTrueColorGetAlpha(c));
      }
      colours_used.clear();

      // set pixels in new image to their palette indexes.
      for (int y = 0; y < sy; ++y)
      {
         for (int x = 0; x < sx; ++x)
         {
            int pal_colour = palette[gdImageTrueColorPixel(img, x, y)];
            gdImageSetPixel(pal_img, x, y, pal_colour);
         }
      }
      
      // update the image pointer to point to the new 
      // palettized image and destroy the old true-colour
      // image.
      *img_ptr = pal_img;
      gdImageDestroy(img);
   }
}

} // anonymous namespace

namespace rendermq {

struct image::pimpl 
{
   gdImagePtr img;
};

image::image(pimpl *impl)
   : m_impl(impl)
{
   if (m_impl->img == NULL) 
   {
      throw std::runtime_error("Image constructed with NULL GD image.");
   }
}

image::~image() 
{
   if (m_impl) 
   {
      gdImageDestroy(m_impl->img);
   }
}

unsigned int image::width() const 
{
   return m_impl->img->sx;
}

unsigned int image::height() const
{
   return m_impl->img->sy;
}

void image::merge(const shared_ptr<image> &other, int x, int y)
{
   // check that alpha blending is turned on for PNG images
   if (gdImageTrueColor(other->m_impl->img))
   {
      // need to set the alpha options for this, as it seems GD won't 
      // handle alpha blending by default, leading to some nice blank
      // images.
      gdImageAlphaBlending(m_impl->img, 1);
      gdImageSaveAlpha(m_impl->img, 1);

      gdImageCopy(m_impl->img, other->m_impl->img, x, y, 
                  0, 0, other->width(), other->height());
   }
   else 
   {
      // merge with full alpha (100 - it's in %) as we are expecting the
      // over image to be partially transparent. of course, if the over
      // image is opaque this will just completely overwrite this image.
      gdImageCopyMerge(m_impl->img, other->m_impl->img, x, y, 
                       0, 0, other->width(), other->height(), 
                       100);
   }
}

string image::save(protoFmt fmt, const bt::ptree &config) const
{
   int size = 0;
   void *bytes = NULL;
   int quality = config.get<int>("jpeg.quality", DEFAULT_JPEG_QUALITY);

   switch (fmt)
   {
   case fmtPNG:
      // try and palettize the image. the existing GD methods
      // for doing this either don't work with the alpha 
      // channel properly, or ignore it entirely, so here we've
      // implemented a very simple method which just squashes
      // down to a palettized image if there are fewer than 256
      // unique colours in the image.
      try_image_palettize(&m_impl->img);
      gdImageSaveAlpha(m_impl->img, 1);
      // experiments show that a zlib compression level of 9 is
      // generally superior...
      bytes = gdImagePngPtrEx(m_impl->img, &size, 9);
      break;

   case fmtJPEG:
      bytes = gdImageJpegPtr(m_impl->img, &size, quality);
      break;

   case fmtGIF:
      bytes = gdImageGifPtr(m_impl->img, &size);
      break;

   default:
      LOG_ERROR(boost::format("Image writer for type %1% (%2%) unknown.")
                % fmt % mime_type_for(fmt));
   }

   if ((bytes != NULL) && (size > 0)) 
   {
      string str((const char *)bytes, (size_t)size);
      gdFree(bytes);
      return str;
   }
   else
   {
      if (bytes != NULL) { gdFree(bytes); }
      throw std::runtime_error("Could not write image.");
   }
}

boost::shared_ptr<image> 
image::create(string &data, protoFmt fmt)
{
   gdImagePtr gd_img;

   switch (fmt)
   {
   case fmtPNG:
      gd_img = gdImageCreateFromPngPtr(data.size(), (void *)data.data());
      break;

   case fmtJPEG:
      gd_img = gdImageCreateFromJpegPtr(data.size(), (void *)data.data());
      break;

   case fmtGIF:
      gd_img = gdImageCreateFromGifPtr(data.size(), (void *)data.data());
      break;

   default:
      LOG_ERROR(boost::format("Image reader for type %1% (%2%) unknown.")
                % fmt % mime_type_for(fmt));
   }

   if (gd_img != NULL) 
   {
      pimpl *impl = new pimpl;
      impl->img = gd_img;
      return shared_ptr<image>(new image(impl));
   }
   else
   {
      LOG_ERROR("Could not construct image.");
      return shared_ptr<image>();
   }
}

boost::shared_ptr<image> image::create_transparent(unsigned int w, unsigned int h)
{
   //make an image
   gdImagePtr gd_img = gdImageCreateTrueColor(w, h);
   if (gd_img == NULL)
   {
      LOG_ERROR("Could not construct image.");
      return shared_ptr<image>();
   }

   //use anti aliasing
   gdImageSaveAlpha(gd_img, 1);
   gdImageAABlend(gd_img);
   
   //set the background color (by creating the first color)
   // note that GD's idea of transparency is somewhat strange -
   // values 0-127 denote transparency levels, and 127 is
   // transparent!
   int transparent = gdImageGetTransparent(gd_img);
   gdImageFill(gd_img, 0, 0, transparent);

   //done with it hand it back
   pimpl *impl = new pimpl;
   impl->img = gd_img;
   return shared_ptr<image>(new image(impl));
}

boost::shared_ptr<image> image::createMillionDotsImage(const std::vector<std::pair<int, int> >& pixelCoords, const int& width, const int& height,
   const tuple<int,int,int,int>& dotColor, const tuple<int,int,int,int>& outlineColor, const tuple<int,int,int,int>& backgroundColor,
   const int& dotSize, const int& outlineThickness)
{
   //make an image
   gdImagePtr gd_img = gdImageCreateTrueColor(width, height);
   if (gd_img == NULL)
   {
      LOG_ERROR("Could not construct image.");
      return shared_ptr<image>();
   }

   //use anti aliasing
   gdImageSaveAlpha(gd_img, 1);
   gdImageAABlend(gd_img);
   
   //set the background color (by creating the first color)
   // note that GD's idea of transparency is somewhat strange -
   // values 0-127 denote transparency levels, and 127 is
   // transparent!
   int transparent = gdImageGetTransparent(gd_img);
   gdImageFill(gd_img, 0, 0, transparent);

   if (backgroundColor.get<3>() != 0)
   {
      int background = gdImageColorAllocateAlpha(gd_img,backgroundColor.get<0>(),backgroundColor.get<1>(),backgroundColor.get<2>(),127 - backgroundColor.get<3>() / 2);
      gdImageFilledRectangle(gd_img, 0, 0, width, height, background);
   }

   //get the other two
   int dot = gdImageColorAllocateAlpha(gd_img,dotColor.get<0>(),dotColor.get<1>(),dotColor.get<2>(),127 - dotColor.get<3>() / 2);
   int outline = gdImageColorAllocateAlpha(gd_img,outlineColor.get<0>(),outlineColor.get<1>(),outlineColor.get<2>(), 127 - outlineColor.get<3>() / 2);

   //for each point
   for(std::vector<std::pair<int, int> >::const_iterator pixel = pixelCoords.begin(); pixel != pixelCoords.end(); pixel++)
   {
      //function sig for drawing ellipses
      //gdImageFilledEllipse(image, pixel x, pixel y, width, height, color)
      //i didn't see a function for drawing arcs with thickness set, it always draws with thickness of 1 pixel

      //border 
      // ARGH this should work. but the gdImageEllipse function, although
      // defined in the header, isn't in the actual library - WTF?
      //gdImageSetThickness(gd_img, outlineThickness);
      //gdImageEllipse(gd_img, pixel->first, pixel->second, dotSize, dotSize, outline);
      gdImageFilledEllipse(gd_img, pixel->first, pixel->second, dotSize + outlineThickness, dotSize + outlineThickness, outline);
      //middle dot
      gdImageFilledEllipse(gd_img, pixel->first, pixel->second, dotSize, dotSize, dot);
   }

   //done with it hand it back
   pimpl *impl = new pimpl;
   impl->img = gd_img;
   return shared_ptr<image>(new image(impl));
}

shared_ptr<image> image::create_from_gd(void *gd_img)
{
   if (gd_img != NULL)
   {
      pimpl *impl = new pimpl;
      impl->img = (gdImagePtr)gd_img;
      return shared_ptr<image>(new image(impl));
   } 
   else
   {
      return shared_ptr<image>();
   }
}

} // rendermq namespace
