/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: matt.amos@mapquest.com
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

#include "test/common.hpp"
#include "test/fake_tile.hpp"
#include "storage/tile_storage.hpp"
#include "storage/disk_storage.hpp"
#include <stdexcept>
#include <iostream>
#include <cstdio>
#include <boost/function.hpp>
#include <boost/format.hpp>
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>
#include <boost/scoped_ptr.hpp>

using boost::function;
using boost::optional;
using boost::shared_ptr;
using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;

using rendermq::cmdRender;
using rendermq::cmdDirty;
using rendermq::cmdStatus;
using rendermq::fmtPNG;
using rendermq::fmtJPEG;
using rendermq::fmtJSON;
using rendermq::disk_storage;
using rendermq::tile_protocol;
using rendermq::tile_storage;

namespace fs = boost::filesystem;

namespace 
{
/* utility class to create a directory and clean up using
 * the RAII idiom.
 */
class tmp_dir
{
public:
   tmp_dir()
   {
      m_dir = fs::path("/tmp") / fs::unique_path();
      if (!fs::create_directories(m_dir))
      {
         throw runtime_error("Cannot create temporary directory for disk tests.");
      }
   }

   ~tmp_dir()
   {
      fs::remove_all(m_dir);
   }

   const fs::path &dir() const
   {
      return m_dir;
   }

private:
   fs::path m_dir;
};

} // anonymous namespace

void test_disk_round_trip_empty() 
{
   tmp_dir tmp;
   disk_storage storage(tmp.dir().native());
   tile_protocol tile;
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data(meta.ptr, meta.total_size), data2;

   if (!storage.put_meta(tile, data)) 
   {
      throw runtime_error("Can't save meta tile!");
   }

   if (!storage.get_meta(tile, data2)) 
   {
      throw runtime_error("Can't load meta tile!");
   }

   if (data != data2) 
   {
      throw runtime_error("Loaded data is different from saved data!");
   }
}

void test_disk_round_trip() 
{
   tmp_dir tmp;
   disk_storage storage(tmp.dir().native());
   tile_protocol tile(cmdRender, 1024, 1024, 12, 0, "osm", fmtPNG, 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data(meta.ptr, meta.total_size), data2;

   if (!storage.put_meta(tile, data)) 
   {
      throw runtime_error("Can't save meta tile!");
   }

   for (int x = 1024; x < 1032; ++x) {
      for (int y = 1024; y < 1032; ++y) {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if (!handle->exists()) 
         {
            throw runtime_error("Tile should exist!");
         }
         if (handle->expired())
         {
            throw runtime_error("Tile should not be expired already!");
         }
      }
   }
}

void test_disk_round_trip_multiformat() 
{
   tmp_dir tmp;
   disk_storage storage(tmp.dir().native());
   tile_protocol tile(cmdRender, 1024, 1024, 12, 0, "osm", (rendermq::protoFmt)(fmtPNG | fmtJPEG), 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data(meta.ptr, meta.total_size), data2;

   if (!storage.put_meta(tile, data)) 
   {
      throw runtime_error("Can't save meta tile!");
   }

   tile.format = fmtPNG;
   for (int x = 1024; x < 1032; ++x) {
      for (int y = 1024; y < 1032; ++y) {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if (!handle->exists()) 
         {
            throw runtime_error("PNG tile should exist!");
         }
         if (handle->expired())
         {
            throw runtime_error("Tile should not be expired already!");
         }
      }
   }

   tile.format = fmtJPEG;
   for (int x = 1024; x < 1032; ++x) {
      for (int y = 1024; y < 1032; ++y) {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if (!handle->exists()) 
         {
            throw runtime_error("JPEG tile should exist!");
         }
         if (handle->expired())
         {
            throw runtime_error("Tile should not be expired already!");
         }
      }
   }
}

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Disk Storage Functions ==" << endl << endl;

   tests_failed += test::run("test_disk_round_trip_empty", &test_disk_round_trip_empty);
   tests_failed += test::run("test_disk_round_trip", &test_disk_round_trip);
   tests_failed += test::run("test_disk_round_trip_multiformat", &test_disk_round_trip_multiformat);
   //tests_failed += test::run("test_", &test_);

   cout << " >> Tests failed: " << tests_failed << endl << endl;

   return 0;
}
