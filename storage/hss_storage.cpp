/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
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

// default driver parameters can be overridden by the
// storage section of the handler / worker configs.
#define DEFAULT_PREFIX "mq"
#define DEFAULT_VERSION "0"
#define DEFAULT_CONCURRENCY (16) //how many HTTP connections to open to the back-end

#include "meta_tile.hpp"
#include "hss_storage.hpp"
#include "../tile_utils.hpp"

// stl
#include <iostream>
#include <sstream>

// boost
#include <boost/foreach.hpp>

using std::time_t;
using std::runtime_error;
using std::cerr;
using std::ostringstream;

namespace rendermq
{

   namespace
   {
      tile_storage* create_hss_storage(boost::property_tree::ptree const& pt,
                                        boost::optional<zmq::context_t &> ctx)
      {
         boost::optional<string> host = pt.get_optional<string>("host");
         boost::optional<unsigned int> port = pt.get_optional<unsigned int>("port");
         boost::optional<string> app_name = pt.get_optional<string>("app_name");
         boost::optional<string> atomics_host = pt.get_optional<string>("atomics_host");
         string prefix = pt.get<string>("prefix", DEFAULT_PREFIX);
         string version = pt.get<string>("version", DEFAULT_VERSION);
         unsigned int concurrency = pt.get<unsigned int>("concurrency", DEFAULT_CONCURRENCY);

         //required
         if(host && port && app_name)
         {
            return new hss_storage(*host, *port, *app_name, prefix, version, atomics_host, concurrency);
         }
         return 0;
      }

      const bool registered = register_tile_storage("hss", create_hss_storage);

   } // anonymous namespace

   hss_storage::hss_storage(string const& host, const unsigned int& port, const string& app_name,  const string &prefix,
      const string& version, boost::optional<string> atomics_host, const bool& persistent, const int &concurrency) :
      http_storage(persistent, concurrency), host(host), port(port), app_name(app_name), prefix(prefix), version(version),
      m_atomics_host(atomics_host)
   {
   }

   hss_storage::~hss_storage()
   {
   }

   const string hss_storage::form_url(const int& x, const int& y, const int& z, const string& style, const protoFmt& format, const bool& isPost, const string &command) const
   {
      return make_host(style, command, isPost) + make_id(x, y, z, format);
   }

   const string hss_storage::make_host(const string& style, const string &command, const bool& isPost) const
   {
      //most hss requests (GET HEAD DELETE) look like this
      //[host]:[port]/hss/storage/[app]/[prefix]:[version]:[style]:[hash]
      //http://hss-prod-b.hss.aol.com/hss/storage/mq/mqmap:1:hyb:f025b00302002
      //however for POST (makes sense I guess) they look like:
      //[host]:[port]/hss/storage/[app]?fileId=[prefix]:[version]:[style]:[hash]
      //http://hss-prod-b.hss.aol.com/hss/storage/mq?fileId=mqmap:1:hyb:f025b00302002
      std::stringstream stream;
      stream << this->host << ":" << this->port 
             << "/hss/" << command << "/" << this->app_name 
             << (isPost ? "?fileId=" : "/");
      stream << this->prefix << ":";
      //we can use these if we ask ops to enable another prefix for us
      stream << this->version << ":" << style << ":";
      return stream.str();
   }

   const string hss_storage::make_id(const int& x, const int& y, const int& z, const protoFmt& format) const
   {
      std::stringstream stream;
      //64 bits to play with where z and format need 8bits and x and y need 20bits
      long id = ((long)format << 48) + ((long)z << 40) + ((long)x << 20) + (long)y;
      stream << std::hex << id;
      return stream.str();
   }

   shared_ptr<tile_storage::handle> hss_storage::get(const tile_protocol &tile) const
   {
      try
      {
         //make the url
         string url = this->form_url(tile.x, tile.y, tile.z, tile.style, tile.format);
         //curl to get the tile data, returns a shared_ptr, forget the last shared pointer we had
         shared_ptr<http::response> response = http::get(url, this->connection);
         //return the response, good or bad
         return shared_ptr<tile_storage::handle> (new handle(response));
      }
      catch(std::runtime_error e)
      {
         LOG_ERROR(boost::format("Runtime error getting HSS tile: %1%") % e.what());
         //return a bad response
         shared_ptr<http::response> response(new http::response());
         return shared_ptr<tile_storage::handle> (new handle(response));
      }
   }

   vector<pair<string, vector<http::part> > > hss_storage::make_put_requests(const tile_protocol &tile,const string &metatile) const
   {
      //the requests
      vector<pair<string, vector<http::part> > > requests;
      //read meta header to get the tile offsets and sizes
      vector<meta_layout*> metaHeaders = read_headers(metatile, fmtAll);
      //figure out the meta tile coordinate
      pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);

      //for each format
      for(vector<meta_layout*>::const_iterator metaHeader = metaHeaders.begin(); metaHeader != metaHeaders.end(); metaHeader++)
      {
         //set the mime type for this set of tiles
         protoFmt format = (protoFmt)(*metaHeader)->fmt;
         const char* mime = rendermq::mime_type_for(format).c_str();
         //for each tile
         for(int i = 0; i < (*metaHeader)->count; i++)
         {
            //only send this if there is an actual tile here
            if((*metaHeader)->index[i].size == 0)
               continue;

            //get the url to post to
            string url = this->form_url(coord.first + (i % METATILE), coord.second + (i / METATILE), tile.z, tile.style, format, true);

            //have to mimic html form post
            vector<http::part> parts;
            parts.push_back(http::part(NULL, 0));
            http::part* part = &parts[0];
            part->name = "file";
            part->mime = mime;
            part->data = metatile.c_str() + (*metaHeader)->index[i].offset; //kind of shady using pointer from a reference parameter
            part->size = (long)(*metaHeader)->index[i].size;
            part->position = 0;
            part->fileName = url.c_str(); //required to trick curl into <input type=file> instead of <input type=text>
            //keep this part
            requests.push_back(make_pair(url, parts));
         }
      }

      //give them back
      return requests;
   }

   vector<string> hss_storage::make_get_requests(const tile_protocol &tile) const
   {
      //get the master tile location
      pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);
      //figure out how many sub tiles it will have
      int size = 1 << tile.z;
      if(METATILE < size)
         size = METATILE;
      vector<string> requests;
      vector<protoFmt> fmts = get_formats_vec(tile.format);

      BOOST_FOREACH(protoFmt fmt, fmts)
      {
         for (int dy = 0; dy < size; ++dy)
         {
            for (int dx = 0; dx < size; ++dx)
            {
               //make the urls
               requests.push_back(form_url(coord.first + dx, coord.second + dy, tile.z, tile.style, fmt));
            }
         }
      }

      return requests;
   }

   bool hss_storage::expire(const tile_protocol &tile) const
   {
      // if we have access to atomics, then use that to do a fast expiry by
      // fiddling with the database directly. this doesn't sound like such
      // a great idea, but the method below is just too slow to be useful in
      // practice.
      if (m_atomics_host)
      {
         int base_x = tile.x & ~(METATILE - 1);
         int base_y = tile.y & ~(METATILE - 1);
         ostringstream sql;
         bool first = true;

         // TODO: really should convert INVALID_TIMESTAMP into a string here.
         sql << "update hss_file set createDate='1970-01-01 00:00:00 +0000', "
             << "touchDate='1970-01-01 00:00:00 +0000' "
             << "where appName='" << app_name << "' and "
             << "fileId in (";
 
         vector<protoFmt> fmts = get_formats_vec(tile.format);
         BOOST_FOREACH(protoFmt fmt, fmts) 
         {
            for (int dy = 0; dy < 8; ++dy) 
            {
               for (int dx = 0; dx < 8; ++dx) 
               {
                  if (first)
                  {
                     first = false; 
                  }
                  else
                  {
                     sql << ","; 
                  }
                  
                  sql << "'" << prefix << ":" << version << ":" << tile.style << ":" 
                      << make_id(base_x + dx, base_y + dy, tile.z, fmt) << "'";
               }
            }
            
         }
         sql << ")";
         
         try 
         {
            shared_ptr<http::response> response = 
               http::get((boost::format("http://%1%/raw/?q=%2%&version=1") 
                          % m_atomics_host.get() % http::escape_url(sql.str(), this->connection)).str());
         }
         catch(std::runtime_error e)
         {
            LOG_ERROR(boost::format("Runtime error while accessing Atomics: %1%") % e.what());
            cerr << e.what() << std::endl;
            return false;
         }
         return true;
      }// if the atomics database isn't directly available to us, then use the /update/ mechanism.
      else
      {
         //TODO: not sure if this should be entire metatile in every format or just single tile single format

         //set the tiles last modified time via the update semantic
         //next time its requested it will have an invalid a timestamp so it will look dirty and force a rerender
         //meanwhile we can still serve it to clients regardless of whether its dirty or not
         try
         {
            int base_x = tile.x & ~(METATILE - 1);
            int base_y = tile.y & ~(METATILE - 1);
            
            vector<protoFmt> fmts = get_formats_vec(tile.format);
            BOOST_FOREACH(protoFmt fmt, fmts) 
            {
               for (int dy = 0; dy < 8; ++dy) 
               {
                  for (int dx = 0; dx < 8; ++dx) 
                  {
                     //make the url
                     string url = form_url(base_x + dx, base_y + dy, tile.z, tile.style, fmt, false, "update");
                     //send it with the bogus last modified time, don't care if it fails or not
                     std::time_t invalid = INVALID_TIMESTAMP;
                     const vector<string> httpHeaders = make_headers(&invalid, (char*)NULL);
                     http::get(url, this->connection, httpHeaders);
                  }
               }
            }
         }
         catch(std::runtime_error e)
         {
            LOG_ERROR(boost::format("Runtime error while expiring HSS tile: %1%") % e.what());
            return false;
         }
         return true;
      }
   }
}

