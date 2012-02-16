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
#include "storage/meta_tile.hpp"
#include "tile_utils.hpp"
#include "storage/tile_storage.hpp"
#include "storage/lts_storage.hpp"
#include <stdexcept>
#include <iostream>
#include <sstream>
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
using rendermq::fmtNone;
using rendermq::fmtGIF;
using rendermq::fmtPNG;
using rendermq::fmtJPEG;
using rendermq::fmtJSON;
using rendermq::fmtAll;
using rendermq::lts_storage;
using rendermq::tile_protocol;
using rendermq::tile_storage;
using rendermq::meta_layout;

#define LTS_TEST_CONFIG "--DISTRIBUTION=consistent --HASH=MURMUR"

#define LTS_TEST_HOST0  "mq-tile-lm01.ihost.aol.com"
#define LTS_TEST_PORT0  (5050)
#define LTS_TEST_HOST1  "mq-tile-ld01.ihost.aol.com"
#define LTS_TEST_PORT1  (5050)
#define LTS_TEST_HOST2  "mq-tile-lm02.ihost.aol.com"
#define LTS_TEST_PORT2  (5050)
#define LTS_TEST_HOST3  "mq-tile-ld02.ihost.aol.com"
#define LTS_TEST_PORT3  (5050)
#define LTS_TEST_HOST4  "mq-tile-lm03.ihost.aol.com"
#define LTS_TEST_PORT4  (5050)
#define LTS_TEST_HOST5  "mq-tile-ld03.ihost.aol.com"
#define LTS_TEST_PORT5  (5050)
#define LTS_TEST_HOST6  "mq-tile-lm04.ihost.aol.com"
#define LTS_TEST_PORT6  (5050)
#define LTS_TEST_HOST7  "mq-tile-ld04.ihost.aol.com"
#define LTS_TEST_PORT7  (5050)
#define LTS_TEST_HOST8  "mq-tile-lm05.ihost.aol.com"
#define LTS_TEST_PORT8  (5050)
#define LTS_TEST_HOST9 "mq-tile-ld05.ihost.aol.com"
#define LTS_TEST_PORT9 (5050)
#define LTS_TEST_HOST10 "mq-tile-lm06.ihost.aol.com"
#define LTS_TEST_PORT10 (5050)
#define LTS_TEST_HOST11 "mq-tile-ld06.ihost.aol.com"
#define LTS_TEST_PORT11 (5050)
#define LTS_TEST_HOST12 "mq-tile-lm07.ihost.aol.com"
#define LTS_TEST_PORT12 (5050)
#define LTS_TEST_HOST13 "mq-tile-ld07.ihost.aol.com"
#define LTS_TEST_PORT13 (5050)
#define LTS_TEST_HOST14 "mq-tile-lm08.ihost.aol.com"
#define LTS_TEST_PORT14 (5050)
#define LTS_TEST_HOST15 "mq-tile-ld08.ihost.aol.com"
#define LTS_TEST_PORT15 (5050)
#define LTS_TEST_HOST16 "mq-tile-lm09.ihost.aol.com"
#define LTS_TEST_PORT16 (5050)
#define LTS_TEST_HOST17 "mq-tile-lm10.ihost.aol.com"
#define LTS_TEST_PORT17 (5050)
#define LTS_TEST_HOST18 "mq-tile-lm11.ihost.aol.com"
#define LTS_TEST_PORT18 (5050)
#define LTS_TEST_HOST19 "mq-tile-lm12.ihost.aol.com"
#define LTS_TEST_PORT19 (5050)
#define LTS_TEST_HOST20 "mq-tile-lm13.ihost.aol.com"
#define LTS_TEST_PORT20 (5050)

namespace
{
   struct fake_tile
   {
         fake_tile(int x, int y, int z, int formats)
         {
            data.clear();
            int tiles = rendermq::get_meta_dimensions(z);
            vector<rendermq::protoFmt> metaFormats = rendermq::get_formats_vec(rendermq::protoFmt(formats));
            data.resize(data.size() + sizeof(struct meta_layout) * metaFormats.size(), 0);

            size_t header_count = 0;
            for(vector<rendermq::protoFmt>::const_iterator fmt = metaFormats.begin(); fmt != metaFormats.end(); ++fmt)
            {
               struct meta_layout *meta = (struct meta_layout *)(&data[0] + header_count * sizeof(struct meta_layout));
               meta->count = METATILE * METATILE;
               meta->magic[0] = 'M';
               meta->magic[1] = 'E';
               meta->magic[2] = 'T';
               meta->magic[3] = 'A';
               meta->x = x;
               meta->y = y;
               meta->z = z;
               meta->fmt = *fmt;
               for(int dy = 0; dy < METATILE; ++dy)
               {
                  for(int dx = 0; dx < METATILE; ++dx)
                  {
                     //only add fake data if we need these tiles at this zoom
                     std::ostringstream ss;
                     if(dx < tiles && dy < tiles)
                     {
                        int bound = int(pow(10, rand() % 6));
                        ss << "[z:" << z << "|x:" << x + dx << "|y:" << y + dy << "|r:" << bound + (rand() % (bound * 9)) << "]";
                     }
                     string tile = ss.str();
                     //printf("%s\n", tile.c_str());
                     meta->index[METATILE * dy + dx].offset = data.size();
                     meta->index[METATILE * dy + dx].size = tile.length();
                     data.insert(data.end(),tile.begin(),tile.end());
                     //we have to regrab this pointer after every allocation
                     meta = (struct meta_layout *)(&data[0] + header_count * sizeof(struct meta_layout));
                  }
               }
               ++header_count;
            }
            //end the data
            //data.push_back('\0');
         }
      
      ~fake_tile() {
      }
      const string GetData()const{return string(data.begin(), data.end());}
      vector<char> data;
   };

} // anonymous namespace

void test_get_partial()
{

}

void test_lts_round_trip_empty()
{
   rendermq::vecHostInfo vecHosts;
   //vecHosts.push_back(std::make_pair("localhost", 5050));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST0, LTS_TEST_PORT0));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST1, LTS_TEST_PORT1));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST2, LTS_TEST_PORT2));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST3, LTS_TEST_PORT3));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST4, LTS_TEST_PORT4));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST5, LTS_TEST_PORT5));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST6, LTS_TEST_PORT6));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST7, LTS_TEST_PORT7));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST8, LTS_TEST_PORT8));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST9, LTS_TEST_PORT9));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST10, LTS_TEST_PORT10));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST11, LTS_TEST_PORT11));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST12, LTS_TEST_PORT12));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST13, LTS_TEST_PORT13));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST14, LTS_TEST_PORT14));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST15, LTS_TEST_PORT15));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST16, LTS_TEST_PORT16));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST17, LTS_TEST_PORT17));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST18, LTS_TEST_PORT18));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST19, LTS_TEST_PORT19));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST20, LTS_TEST_PORT20));

   lts_storage storage(vecHosts, LTS_TEST_CONFIG, "mq", "0", 128);

   tile_protocol tile;
   tile.style = "test_rte-0";
   tile.format = fmtGIF;
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);

   string data=meta.GetData(), data2;

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   if(!storage.get_meta(tile, data2))
      throw runtime_error("Can't load meta tile!");

   if(data != data2)
      throw runtime_error("Loaded data is different from saved data!");
}

void test_lts_round_trip_multiformat()
{
   rendermq::vecHostInfo vecHosts;
   //vecHosts.push_back(std::make_pair("localhost", 5050));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST0, LTS_TEST_PORT0));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST1, LTS_TEST_PORT1));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST2, LTS_TEST_PORT2));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST3, LTS_TEST_PORT3));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST4, LTS_TEST_PORT4));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST5, LTS_TEST_PORT5));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST6, LTS_TEST_PORT6));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST7, LTS_TEST_PORT7));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST8, LTS_TEST_PORT8));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST9, LTS_TEST_PORT9));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST10, LTS_TEST_PORT10));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST11, LTS_TEST_PORT11));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST12, LTS_TEST_PORT12));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST13, LTS_TEST_PORT13));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST14, LTS_TEST_PORT14));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST15, LTS_TEST_PORT15));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST16, LTS_TEST_PORT16));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST17, LTS_TEST_PORT17));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST18, LTS_TEST_PORT18));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST19, LTS_TEST_PORT19));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST20, LTS_TEST_PORT20));

   lts_storage storage(vecHosts, LTS_TEST_CONFIG, "mq", "0", 128);
   tile_protocol tile(cmdRender, 0, 0, 2, 0, "test_rtmf-1", (rendermq::protoFmt)(fmtPNG | fmtJPEG/*| fmtGIF | fmtJSON*/), 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data=meta.GetData(),data2;

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   //get the meta in one shot
   if(!storage.get_meta(tile, data2))
      throw runtime_error("Can't load meta tile!");

   if(data != data2)
      throw runtime_error("Loaded data is different from saved data!");

   //for all formats
   int dim = rendermq::get_meta_dimensions(tile.z);
   std::pair<int, int> base = rendermq::xy_to_meta_xy(tile.x,tile.y);
   vector<rendermq::protoFmt> formats = rendermq::get_formats_vec(tile.format);
   for(vector<rendermq::protoFmt>::const_iterator format = formats.begin(); format != formats.end(); format++)
   {
      //get all the tiles for this format
      tile.format = *format;
      for(tile.y = base.second; tile.y < dim + base.second; tile.y++)
      {
         for(tile.x = base.first; tile.x < dim + base.first; tile.x++)
         {
            shared_ptr<tile_storage::handle> handle = storage.get(tile);
            if(!handle->exists())
               throw runtime_error("Tile should exist!");
            if(handle->expired())
               throw runtime_error("Tile should not be expired already!");
            //check that the data is right!
            std::ostringstream tileData;
            tileData << "[z:" << tile.z << "|x:" << tile.x << "|y:" << tile.y << "|r:";
            handle->data(data2);
            if(data2.substr(0, tileData.str().length()) != tileData.str())
               throw runtime_error("Unexpected tile data!");
         }
      }
   }
}

void test_lts_expire_meta()
{
   rendermq::vecHostInfo vecHosts;
   //vecHosts.push_back(std::make_pair("localhost", 5050));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST0, LTS_TEST_PORT0));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST1, LTS_TEST_PORT1));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST2, LTS_TEST_PORT2));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST3, LTS_TEST_PORT3));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST4, LTS_TEST_PORT4));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST5, LTS_TEST_PORT5));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST6, LTS_TEST_PORT6));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST7, LTS_TEST_PORT7));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST8, LTS_TEST_PORT8));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST9, LTS_TEST_PORT9));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST10, LTS_TEST_PORT10));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST11, LTS_TEST_PORT11));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST12, LTS_TEST_PORT12));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST13, LTS_TEST_PORT13));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST14, LTS_TEST_PORT14));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST15, LTS_TEST_PORT15));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST16, LTS_TEST_PORT16));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST17, LTS_TEST_PORT17));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST18, LTS_TEST_PORT18));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST19, LTS_TEST_PORT19));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST20, LTS_TEST_PORT20));

   lts_storage storage(vecHosts, LTS_TEST_CONFIG, "mq", "0", 128);
   tile_protocol tile(cmdRender, 32, 40, 7, 0, "test_em-2", rendermq::protoFmt(fmtPNG | fmtJPEG /*| fmtGIF | fmtJSON*/ ), 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data=meta.GetData(), data2;

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   if (!storage.expire(tile))
      throw runtime_error("Can't expire tile!");

   //get the meta in one shot
   if(storage.get_meta(tile, data2))
      throw runtime_error("Get meta should fail on expired tiles!");

   //for all formats
   int dim = rendermq::get_meta_dimensions(tile.z);
   std::pair<int, int> base = rendermq::xy_to_meta_xy(tile.x,tile.y);
   vector<rendermq::protoFmt> formats = rendermq::get_formats_vec(tile.format);
   for(vector<rendermq::protoFmt>::const_iterator format = formats.begin(); format != formats.end(); format++)
   {
      //get all the tiles for this format
      tile.format = *format;
      for(tile.y = base.second; tile.y < dim + base.second; tile.y++)
      {
         for(tile.x = base.first; tile.x < dim + base.first; tile.x++)
         {
            shared_ptr<tile_storage::handle> handle = storage.get(tile);
            if(!handle->exists())
               throw runtime_error("Tile should exist!");
            if(!handle->expired())
               throw runtime_error("Tile should be expired!");
            //check that the data is right!
            std::ostringstream tileData;
            tileData << "[z:" << tile.z << "|x:" << tile.x << "|y:" << tile.y << "|r:";
            handle->data(data2);
            if(data2.substr(0, tileData.str().length()) != tileData.str())
               throw runtime_error("Unexpected tile data!");
         }
      }
   }
}

namespace rendermq
{
   //to get access to protected member functions we cheat and derive from the base
   class lts_storage_tester: public lts_storage
   {
      public:
         //make sure to init base class
         lts_storage_tester(const vecHostInfo& vecHosts,const string& config, const string& app_name, const string& version, const int& concurrency = 1):
            lts_storage(vecHosts, config, app_name, version, concurrency)
         {}
         //wrap the protected url generation fuction so we can get it manually to look at the headers
         string getTileURL(const tile_protocol& tile)const
         {
            return this->form_url(tile.x,tile.y,tile.z,tile.style,tile.format);
         }
   };
}

void test_lts_mime_type()
{
   rendermq::vecHostInfo vecHosts;
   //vecHosts.push_back(std::make_pair("localhost", 5050));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST0, LTS_TEST_PORT0));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST1, LTS_TEST_PORT1));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST2, LTS_TEST_PORT2));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST3, LTS_TEST_PORT3));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST4, LTS_TEST_PORT4));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST5, LTS_TEST_PORT5));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST6, LTS_TEST_PORT6));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST7, LTS_TEST_PORT7));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST8, LTS_TEST_PORT8));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST9, LTS_TEST_PORT9));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST10, LTS_TEST_PORT10));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST11, LTS_TEST_PORT11));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST12, LTS_TEST_PORT12));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST13, LTS_TEST_PORT13));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST14, LTS_TEST_PORT14));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST15, LTS_TEST_PORT15));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST16, LTS_TEST_PORT16));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST17, LTS_TEST_PORT17));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST18, LTS_TEST_PORT18));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST19, LTS_TEST_PORT19));
   vecHosts.push_back(std::make_pair(LTS_TEST_HOST20, LTS_TEST_PORT20));

   rendermq::lts_storage_tester storage(vecHosts, LTS_TEST_CONFIG, "mq", "0", 128);

   rendermq::protoFmt formats = (rendermq::protoFmt)(fmtPNG | fmtJPEG | fmtGIF | fmtJSON);
   tile_protocol tile(cmdRender, 1024, 1024, 12, 0, "test_mt-3", formats, 0, 0);
   fake_tile meta(tile.x, tile.y, tile.z, tile.format);
   string data=meta.GetData();

   if(!storage.put_meta(tile, data))
      throw runtime_error("Can't save meta tile!");

   //for all formats
   for(tile.format = (rendermq::protoFmt)(fmtNone + 1); tile.format < fmtAll; tile.format = (rendermq::protoFmt)(tile.format << 1))
   {
      //if we sent this format
      if(formats & tile.format)
      {
         //get all this tile
         tile.x = tile.y = 1024;
         boost::shared_ptr<http::response> response = http::get(storage.getTileURL(tile),boost::shared_ptr<CURL>(),std::vector<std::string>(),true);
         //check if any of its headers have the mime type we were expecting
         string mime = rendermq::mime_type_for(tile.format );
         bool passed = false;
         for(vector<string>::const_iterator header = response->headers.begin(); !passed && header != response->headers.end(); header++)
         {
            if(header->find(mime) != string::npos)
               passed = true;
         }
         if(!passed)
         {
            string message = "Response did not contain expected mime type: ";
            throw runtime_error(message + mime);
         }
      }
   }
}

int main()
{
   int tests_failed = 0;

   cout << "== Testing LTS Storage Functions ==" << endl << endl;

   tests_failed += test::run("test_lts_round_trip_empty", &test_lts_round_trip_empty);
   tests_failed += test::run("test_lts_round_trip_multiformat", &test_lts_round_trip_multiformat);
   tests_failed += test::run("test_lts_expire_meta", &test_lts_expire_meta);
   tests_failed += test::run("test_lts_mime_type", &test_lts_mime_type);

   cout << " >> Tests failed: " << tests_failed << endl << endl;

   return 0;
}
