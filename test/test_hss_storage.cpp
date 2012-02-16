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
#include "storage/hss_storage.hpp"
#include <stdexcept>
#include <iostream>
#include <cstdio>
#include <boost/function.hpp>
#include <boost/format.hpp>
#define BOOST_FILESYSTEM_VERSION 3
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
using rendermq::fmtGIF;
using rendermq::fmtPNG;
using rendermq::fmtJPEG;
using rendermq::fmtJSON;
using rendermq::fmtAll;
using rendermq::hss_storage;
using rendermq::tile_protocol;
using rendermq::tile_storage;

#define HSS_TEST_HOST "http://hss-prod-b.hss.aol.com"
#define HSS_TEST_PORT (80)
#define HSS_TEST_APP_NAME "mq"
#define HSS_TEST_PREFIX "mqmap"
#define HSS_TEST_VERSION "1"

void test_hss_round_trip_empty()
{
   const std::string host = HSS_TEST_HOST;
   const unsigned int port = HSS_TEST_PORT;
   const std::string app_name = HSS_TEST_APP_NAME;
   const std::string prefix = HSS_TEST_PREFIX;
   const std::string version = HSS_TEST_VERSION;
   hss_storage storage(host, port, app_name, prefix, version, boost::optional<string>(), true, 128);

   tile_protocol tile;
   tile.style = "tst";
   tile.format = fmtGIF;
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);

   string data(meta.ptr, meta.total_size), data2;

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   if(!storage.get_meta(tile, data2))
      throw runtime_error("Can't load meta tile!");

   if(data != data2)
      throw runtime_error("Loaded data is different from saved data!");
}

void test_hss_round_trip_multiformat()
{
   const std::string host = HSS_TEST_HOST;
   const unsigned int port = HSS_TEST_PORT;
   const std::string app_name = HSS_TEST_APP_NAME;
   const std::string prefix = HSS_TEST_PREFIX;
   const std::string version = HSS_TEST_VERSION;
   hss_storage storage(host, port, app_name, prefix, version, boost::optional<string>(), true, 128);

   tile_protocol tile(cmdRender, 1024, 1024, 12, 0, "tst", (rendermq::protoFmt)(fmtPNG | fmtJPEG), 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data(meta.ptr, meta.total_size);

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   //unfortunately we have to wait 2 minutes here because of the way hss does the database update
   //hss has a caching mechanism which will cache the response for this tile
   //when we perform an update (expire) the cache doesn't get the update so
   //we have to wait 2 minutes before the cache expires and hss is forced to actually go to the db
   //and get the updated file. we've since disabled caching and can disable this
   //sleep(121);

   tile.format = fmtPNG;
   for(int x = 1024; x < 1032; ++x)
   {
      for(int y = 1024; y < 1032; ++y)
      {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if(!handle->exists())
            throw runtime_error("PNG tile should exist!");
         if (handle->expired())
            throw runtime_error("Tile should not be expired already!");
      }
   }

   tile.format = fmtJPEG;
   for(int x = 1024; x < 1032; ++x)
   {
      for(int y = 1024; y < 1032; ++y)
      {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if(!handle->exists())
            throw runtime_error("JPEG tile should exist!");
         if (handle->expired())
            throw runtime_error("Tile should not be expired already!");
      }
   }
}

void test_hss_expire_meta()
{
   const std::string host = HSS_TEST_HOST;
   const unsigned int port = HSS_TEST_PORT;
   const std::string app_name = HSS_TEST_APP_NAME;
   const std::string prefix = HSS_TEST_PREFIX;
   const std::string version = HSS_TEST_VERSION;
   hss_storage storage(host, port, app_name, prefix, version, boost::optional<string>(), true, 128);

   tile_protocol tile(cmdRender, 1024, 1024, 12, 0, "tst", (rendermq::protoFmt)(fmtPNG | fmtJPEG), 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data(meta.ptr, meta.total_size), data2;

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   if (!storage.expire(tile))
      throw runtime_error("Can't expire tile!");

   //unfortunately we have to wait 2 minutes here because of the way hss does the database update
   //hss has a caching mechanism which will cache the response for this tile
   //when we perform an update (expire) the cache doesn't get the update so
   //we have to wait 2 minutes before the cache expires and hss is forced to actually go to the db
   //and get the updated file. we've since disabled caching and can disable this
   //sleep(121);

   tile.format = fmtPNG;
   for(int x = 1024; x < 1032; ++x)
   {
      for(int y = 1024; y < 1032; ++y)
      {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if(!handle->exists())
            throw runtime_error("PNG tile should exist!");
         if (!handle->expired())
            throw runtime_error("PNG tile should be expired!");
      }
   }

   tile.format = fmtJPEG;
   for(int x = 1024; x < 1032; ++x)
   {
      for(int y = 1024; y < 1032; ++y)
      {
         tile.x = x;
         tile.y = y;
         shared_ptr<tile_storage::handle> handle = storage.get(tile);
         if(!handle->exists())
            throw runtime_error("JPEG tile should exist!");
         if (!handle->expired())
            throw runtime_error("JPEG tile should be expired!");
      }
   }
}

int main()
{
   int tests_failed = 0;

   cout << "== Testing HSS Storage Functions ==" << endl << endl;

   tests_failed += test::run("test_hss_round_trip_empty", &test_hss_round_trip_empty);
   tests_failed += test::run("test_hss_round_trip_multiformat", &test_hss_round_trip_multiformat);
   tests_failed += test::run("test_hss_expire_meta", &test_hss_expire_meta);

   cout << " >> Tests failed: " << tests_failed << endl << endl;

   return 0;
}
