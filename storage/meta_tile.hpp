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

#ifndef META_TILE_HPP
#define META_TILE_HPP

#include <string>
#include <boost/array.hpp>
#include <vector>
#include "../tile_utils.hpp"

// how wide and high a metatile is, in tiles
#define METATILE 8 

namespace rendermq
{

   struct entry
   {
         int offset;
         int size;
   };

   struct meta_layout
   {
         char magic[4];
         int count;
         int x, y, z, fmt;
         boost::array<entry, METATILE * METATILE> index;

         bool magic_ok()
         {
            return ((magic[0] == 'M') && (magic[1] == 'E') && (magic[2] == 'T') && (magic[3] == 'A'));
         }
   };

   class metaTile
   {
      public:
         metaTile(int x, int y, int z, std::string const &style);
         void clear();
         void set(int x, int y, const std::string &data);
         std::string const& get(int x, int y) const;
         void save(std::string const& tile_dir, std::string const& hostname);
#ifdef HTCP_EXPIRE_CACHE
         void expire_tiles(int sock, char * host, char * uri);
#endif
         int x_, y_, z_;
         std::string style_;
         std::string tile[METATILE][METATILE];
      static const int header_size = sizeof(struct meta_layout);

   };

   class metatile_reader
   {
      public:
         typedef std::string::const_iterator iterator_type;
         metatile_reader(const std::string &data, int fmt);
         std::pair<iterator_type, iterator_type> get(int x, int y) const;

         meta_layout header_;
         const char * data_;
         size_t size_;
         bool initialized_;
   };

   std::pair<std::string, int> xyz_to_meta(std::string const& tile_dir, int x, int y, int z, std::string const &style);
   std::pair<int, int> xy_to_meta_xy(const int& x, const int& y);
   int xyz_to_meta_offset(const int& x, const int& y, const int& z);
   int get_meta_dimensions(const int& zoom, const int& limit = METATILE);
   int get_tile_count_in_meta(const int& zoom, const int& limit = METATILE);

   std::vector<meta_layout*> read_headers(const std::string& buf, const int& formatMask);
   std::string write_headers(const int& x, const int& y, const int& z, const std::vector<protoFmt>& formats, const std::vector<int>& sizes);
   int read_from_meta(std::string const& tile_dir, int x, int y, int z, std::string const &style, unsigned char* buf,
            size_t sz, int fmt);

}

#endif // META_TILE_HPP

