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
#include <cstdarg>
#include "simple_http_storage.hpp"
#include "null_handle.hpp"
#include "meta_tile.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>

namespace bt = boost::posix_time;

namespace 
{

rendermq::tile_storage *create_simple_http_storage(boost::property_tree::ptree const &pt,
                                                   boost::optional<zmq::context_t &> ctx)
{
   boost::optional<string> url = pt.get_optional<string>("url");

   if (url)
   {
      return new rendermq::simple_http_storage(url.get());
   }
   else
   {
      return 0;
   }
}

const bool registered = register_tile_storage("simple_http", create_simple_http_storage);

} // anonymous namespace

namespace rendermq
{

simple_http_storage::handle::handle(const shared_ptr<http::response>& response) : response(response)
{
}

simple_http_storage::handle::~handle()
{
}

bool simple_http_storage::handle::exists() const
{
   //its there if we got it ok
   return this->response->statusCode == 200;
}

std::time_t simple_http_storage::handle::last_modified() const
{
   //this is set to INVALID_TIMESTAMP when timestamp info doesn't come back in http header
   return this->response->timeStamp;
}

bool simple_http_storage::handle::expired() const
{
   //if we didn't get a tile back or it didn't have a timestamp
   return this->response->timeStamp == INVALID_TIMESTAMP || this->response->statusCode != 200;
}

bool simple_http_storage::handle::data(string &output) const
{
   //give it the body of the response
   output = this->response->body;
   return this->response->statusCode == 200;
}

simple_http_storage::simple_http_storage(const string &format)
   : m_format(format),
     m_connection(http::createPersistentConnection())
{
}

simple_http_storage::~simple_http_storage()
{
}

shared_ptr<tile_storage::handle> 
simple_http_storage::get(const tile_protocol &tile) const 
{
   string url = make_url(tile.style, tile.z, tile.x, tile.y);
   shared_ptr<http::response> response = http::get(url, m_connection);
   if (response->statusCode == 200)
   {
      return shared_ptr<tile_storage::handle>(new handle(response));
   }
   else
   {
      return shared_ptr<tile_storage::handle>(new null_handle());
   }
}

bool 
simple_http_storage::get_meta(const tile_protocol &tile, string &data) const
{
   //get the master tile location
   pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);
   //figure out how many sub tiles it will have
   int size = 1 << tile.z;
   if(METATILE < size)
      size = METATILE;
   //place to keep the formats
   vector<protoFmt> formats = rendermq::get_formats_vec(tile.format);
   //place to keep sizes
   vector<int> sizes(formats.size() * METATILE * METATILE, 0);
   vector<int>::iterator tileSize = sizes.begin();
   //formats
   for(vector<protoFmt>::const_iterator f = formats.begin(); f != formats.end(); f++)
   {
      //rows
      for(int y = coord.first; y < coord.first + size; y++)
      {
         //columns
         for(int x = coord.second; x < coord.second + size; x++)
         {
            try
            {
               string url = make_url(tile.style, tile.z, x, y);
               shared_ptr<http::response> response = http::get(url, m_connection);
               if(response->statusCode != 200 || response->timeStamp == INVALID_TIMESTAMP)
                  return false;

               data += response->body;
               *tileSize = int(response->body.length()); tileSize++;
            }
            catch(std::runtime_error e)
            {
               LOG_ERROR(boost::format("Runtime error while getting LTS (meta) tile: %1%") % e.what());
               return false;
            }
         }
      }
   }
   //add the meta headers
   data += '\0';
   data.insert(0, write_headers(coord.first, coord.second, tile.z, formats, sizes));
   return true;
}

bool 
simple_http_storage::put_meta(const tile_protocol &tile, const string &metatile) const
{
   return false;
}

bool 
simple_http_storage::expire(const tile_protocol &tile) const 
{
   return false;
}

string
simple_http_storage::make_url(const string &style, int z, int x, int y) const
{
   return (boost::format(m_format) % style % z % x % y).str();
}

}

