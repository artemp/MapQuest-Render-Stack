/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: artem@mapnik-consulting.com
 *  Author: kevin.kreiser@mapquest.com
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

// boost
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
// stl
#include <iostream>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include "meta_tile.hpp"
#include "../logging/logger.hpp"

#include <cstring> // for strlen
#define META_MAGIC "META"

namespace rendermq
{

   std::pair<std::string, int> xyz_to_meta(std::string const& tile_dir, int x, int y, int z, std::string const &style)
   {
      unsigned hash[5];
      unsigned mask = METATILE - 1;
      unsigned offset = (y & mask) * METATILE + (x & mask);
      x &= ~mask;
      y &= ~mask;
      for(unsigned i = 0; i < 5; i++)
      {
         hash[i] = ((x & 0x0f) << 4) | (y & 0x0f);
         x >>= 4;
         y >>= 4;
      }
      std::string path = (boost::format("%s/%s/%d/%u/%u/%u/%u/%u.meta") % tile_dir % style % z % hash[4] % hash[3] % hash[2]
               % hash[1] % hash[0]).str();
      return std::make_pair(path, offset);
   }

   int xyz_to_meta_offset(const int& x, const int& y, const int& z)
   {
      unsigned char mask = METATILE - 1;
      return (y & mask) * METATILE + (x & mask);
   }

   std::pair<int, int> xy_to_meta_xy(const int& x, const int& y)
   {
      unsigned mask = METATILE - 1;
      mask = ~mask;
      return std::make_pair(x & mask, y & mask);
   }

   int get_meta_dimensions(const int& zoom, const int& limit)
   {
      return std::min(1 << zoom, limit);
   }

   int get_tile_count_in_meta(const int& zoom, const int& limit)
   {
      int dim = get_meta_dimensions(zoom, limit);
      return dim * dim;
   }

   std::vector<meta_layout*> read_headers(const std::string& buf, const int& formatMask)
   {
      std::vector<meta_layout*> headers;
      //check each consecutive header-sized block to see if it is a header
      for(size_t offset = 0; buf.length() - offset > sizeof(meta_layout) - 1; offset += sizeof(meta_layout))
      {
         //make the struct out of this portion of the data
         struct meta_layout* header = (struct meta_layout *)&buf[offset];
         //keep the header if it is a reasonable format and has the magic word
         if((header->fmt & formatMask) && header->magic_ok())
            headers.push_back(header);
         //this was a bogus header (or we're into tile data)
         else
            break;
      }

      //return whatever we've got
      return headers;
   }

   std::string write_headers(const int& x, const int& y, const int& z, const std::vector<protoFmt>& formats, const std::vector<int>& sizes)
   {
      //create a header
      struct meta_layout header;
      //memset(&header, 0, sizeof(header));
      //should we consider the zoom level and not have all metatiles have the same number of tiles no matter what?
      header.count = METATILE * METATILE;
      memcpy(header.magic, META_MAGIC, strlen(META_MAGIC));
      header.x = x;
      header.y = y;
      header.z = z;

      //bail if this isn't going to work
      if(sizes.size() != formats.size() * header.count)
      {
         std::ostringstream ostr;
         ostr << "Received " << sizes.size() << " tile sizes for metatile headers, " << formats.size() * header.count << " required";
         throw std::runtime_error(ostr.str());
      }

      //keep track of which size we are on
      std::vector<int>::const_iterator size = sizes.begin();

      //the first tile will end up after all the headers
      int offset = int(formats.size() * sizeof(header));

      //for each format
      std::string headers;
      for(std::vector<protoFmt>::const_iterator f = formats.begin(); f != formats.end(); f++)
      {
         //set the format
         header.fmt = *f;
         //calculate the offsets and sizes
         for(int i = 0; i < header.count; i++, size++)
         {
            //how big is this tile
            header.index[i].size = *size;
            //where does it start from the beginning of the meta tile
            header.index[i].offset = offset;
            //where will the next tile go
            offset = header.index[i].offset + *size;
         }
         //add the header
         headers.append((const char*)&header, sizeof(header));
      }

      //return the headers
      return headers;
   }

   int read_from_meta(std::string const& tile_dir, int x, int y, int z, std::string const &style, unsigned char* buf,
            size_t sz, int fmt)
   {
      char header[4096];
      std::pair<std::string, int> metatile = xyz_to_meta(tile_dir, x, y, z, style);

      int fd = open(metatile.first.c_str(), O_RDONLY);
      if(fd < 0)
         return -1;

      unsigned pos = 0;
      while(pos < sizeof(header))
      {
         size_t len = sizeof(header) - pos;
         int got = read(fd, header + pos, len);
         if(got < 0)
         {
            close(fd);
            return -2;
         }
         else
            if(got > 0)
            {
               pos += got;
            }
            else
            {
               break;
            }
      }

      // search for the correct format metatile header.
      size_t n_header = 0;
      struct meta_layout *m = NULL;
      do
      {
         m = (struct meta_layout *)(header + n_header * metaTile::header_size);
         if(pos < (n_header + 1) * metaTile::header_size)
         {
            LOG_ERROR(boost::format("Meta file %1% too small to contain header") % metatile.first);
            return -3;
         }
         if(memcmp(m->magic, META_MAGIC, strlen(META_MAGIC)))
         {
            LOG_WARNING(boost::format("Meta file %1% header magic mismatch") % metatile.first);
            return -4;
         }
         ++n_header;
      }while(m->fmt != fmt);

      // Currently this code only works with fixed metatile sizes (due to xyz_to_meta above)
      if(m->count != (METATILE * METATILE))
      {
         LOG_WARNING(boost::format("Meta file %1% header bad count %2% != %3%")
                     % metatile.first % m->count % (METATILE * METATILE));
         return -5;
      }

      size_t file_offset = m->index[metatile.second].offset;
      size_t tile_size = m->index[metatile.second].size;

      if(lseek(fd, file_offset, SEEK_SET) < 0)
      {
         LOG_ERROR(boost::format("Meta file %1% seek error %2%") % metatile.first % m->count);
         return -6;
      }
      if(tile_size > sz)
      {
         LOG_WARNING(boost::format("Truncating tile %1% to fit buffer of %1%") % tile_size % sz);
         tile_size = sz;
      }
      pos = 0;
      while(pos < tile_size)
      {
         size_t len = tile_size - pos;
         int got = read(fd, buf + pos, len);
         if(got < 0)
         {
            close(fd);
            return -7;
         }
         else
            if(got > 0)
            {
               pos += got;
            }
            else
            {
               break;
            }
      }
      close(fd);
      return pos;
   }

   metaTile::metaTile(int x, int y, int z, std::string const &style) :
      x_(x), y_(y), z_(z), style_(style)
   {
      clear();
   }

   void metaTile::clear()
   {
      for(int x = 0; x < METATILE; x++)
         for(int y = 0; y < METATILE; y++)
            tile[x][y] = "";
   }

   void metaTile::set(int x, int y, const std::string &data)
   {
      tile[x][y] = data;
   }

   std::string const& metaTile::get(int x, int y) const
   {
      return tile[x][y];
   }

   void metaTile::save(std::string const& tile_dir, std::string const& hostname)
   {
      int ox, oy, limit;
      size_t offset;
      struct entry offsets[METATILE * METATILE];
      struct meta_layout m;
      memset(&m, 0, sizeof(m));
      memset(&offsets, 0, sizeof(offsets));

      std::pair<std::string, int> metatile = xyz_to_meta(tile_dir, x_, y_, z_, style_);
      std::stringstream ss;
      ss << metatile.first << "." << std::string(hostname) << "." << pthread_self();
      std::string tmp(ss.str());

      boost::filesystem::path p0(tmp);
      boost::filesystem::create_directories(p0.parent_path());

      std::ofstream file(tmp.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);

      // Create and write header
      m.count = METATILE * METATILE;
      memcpy(m.magic, META_MAGIC, strlen(META_MAGIC));
      m.x = x_;
      m.y = y_;
      m.z = z_;
      file.write((const char *)&m, sizeof(m));

      offset = header_size;
      limit = get_meta_dimensions(z_);

      // Generate offset table
      for(ox = 0; ox < limit; ox++)
      {
         for(oy = 0; oy < limit; oy++)
         {
            int mt = xyz_to_meta_offset(x_ + ox, y_ + oy, z_);
            offsets[mt].offset = offset;
            offsets[mt].size = tile[ox][oy].size();
            offset += offsets[mt].size;
         }
      }
      file.write((const char *)&offsets, sizeof(offsets));

      // Write tiles
      for(ox = 0; ox < limit; ox++)
      {
         for(oy = 0; oy < limit; oy++)
         {
            file.write((const char *)tile[ox][oy].data(), tile[ox][oy].size());
         }
      }

      file.close();
      rename(tmp.c_str(), metatile.first.c_str());
   }

   /* even if the offset is > 0 the offsets in the entries are still
    * relative to the beginning of the *file*, not relative to the
    * metatile header. */
   metatile_reader::metatile_reader(const std::string &data, int fmt):data_(data.c_str()), size_(data.size()), initialized_(false)
   {
      int offset = 0;
      // now that metatiles might have some arbitrary number of
      // headers, one for each format, we have to be a little more
      // complex about the way that we read metatiles. so, loop
      // through all the headers:
      do
      {
         if(size_ < (offset + 1) * sizeof(header_))
            break;

         std::copy(data_ + offset * sizeof(header_), data_ + (offset + 1) * sizeof(header_), reinterpret_cast<char*> (&header_));

         // exit when the one that is being searched for is found.
         if(header_.fmt == fmt)
         {
            initialized_ = header_.magic_ok();
            break;
         }

         // and keep looking while the headers still look like
         // metatile headers.
         ++offset;
      }while(header_.magic_ok());
   }

   std::pair<metatile_reader::iterator_type, metatile_reader::iterator_type> metatile_reader::get(int x, int y) const
   {
      if(initialized_)
      {
         unsigned mask = METATILE - 1;
         unsigned offset = (y & mask) * METATILE + (x & mask);
         size_t tile_offset = header_.index[offset].offset;
         size_t tile_size = header_.index[offset].size;

         if(tile_offset + tile_size <= size_)
         {
            return std::make_pair(data_ + tile_offset, data_ + tile_offset + tile_size);
         }
      }
      return std::make_pair(data_ + size_, data_ + size_);
   }

}

