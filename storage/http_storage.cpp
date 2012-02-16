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
#include <cstdarg>
#include "http_storage.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>

namespace bt = boost::posix_time;

namespace rendermq
{

   http_storage::handle::handle(const shared_ptr<http::response>& response) : response(response)
   {
   }

   http_storage::handle::~handle()
   {
   }

   bool http_storage::handle::exists() const
   {
      //its there if we got it ok
      return this->response->statusCode == 200;
   }

   std::time_t http_storage::handle::last_modified() const
   {
      //this is set to INVALID_TIMESTAMP when timestamp info doesn't come back in http header
      return this->response->timeStamp;
   }

   bool http_storage::handle::expired() const
   {
      //if we didn't get a tile back or it didn't have a timestamp
      return this->response->timeStamp == INVALID_TIMESTAMP || this->response->statusCode != 200;
   }

   bool http_storage::handle::data(string &output) const
   {
      //give it the body of the response
      output = this->response->body;
      return this->response->statusCode == 200;
   }

   http_storage::http_storage(const bool& persistent, const int& concurrency): persistent(persistent), concurrency(concurrency)
   {
      //get a persistent connection
      if(persistent)
         this->connection = http::createPersistentConnection();
   }

   http_storage::~http_storage()
   {
   }

   bool http_storage::put_meta(const tile_protocol &tile, const string &metatile) const
   {
      //put extra stuff in the http header
      std::time_t now = std::time(0);
      vector<string> headers = this->make_headers(&now, (char*)NULL);
      //get the put requests
      vector<pair<string, vector<http::part> > > requests = make_put_requests(tile, metatile);

#ifdef RENDERMQ_DEBUG
      LOG_FINER(boost::format("Storing metatile: %1%") % tile);
      bt::ptime start = bt::microsec_clock::local_time();
#endif

      //if we aren't using concurrency
      bool ret = (concurrency < 2 ? put_meta_serial(requests, headers) : put_meta_parallel(requests, headers));

#ifdef RENDERMQ_DEBUG
      bt::ptime end = bt::microsec_clock::local_time();
      LOG_FINER(boost::format("Finished storing metatile: %1% in %2% secs") % tile % (end - start));
#endif

      //if we failed for any reason, just make the tile dirty don't waste time deleting it
      if(ret == false)
      {
         LOG_FINER("Tile store failed, expiring.");
         this->expire(tile);
      }

      //return whether we were successful
      return ret;
   }

   bool http_storage::put_meta_parallel(const vector<pair<string, vector<http::part> > >& requests, const vector<string>& headers) const
   {
      //do the requests
      vector<shared_ptr<http::response> > responses;
      try
      {
         responses = http::multiPostForm(requests, concurrency, connection, headers);
      }
      catch(std::runtime_error e)
      {
         LOG_ERROR(boost::format("Runtime error asynchronously PUTTING tile: %1%") % e.what());
         return false;
      }

      //assume we will be good
      bool ret = true;
      //check the responses of each request
      vector<shared_ptr<http::response> >::const_iterator itr = responses.begin();
      vector<pair<string, vector<http::part> > >::const_iterator jtr = requests.begin();
      for (; itr != responses.end(); ++itr, ++jtr)
      {
         if((*itr)->statusCode != 200)
         {
            LOG_ERROR(boost::format("Failed to PUT tile: %1% (status=%2%)") % jtr->first % (*itr)->statusCode);
            ret = false;
         }
      }

      //return whether we were successful
      return ret;
   }

   bool http_storage::put_meta_serial(const vector<pair<string, vector<http::part> > >& requests, const vector<string>& headers) const
   {
      //assume we will be good
      bool ret = true;
      //do each request and check the response
      for(vector<pair<string, vector<http::part> > >::const_iterator request = requests.begin(); request != requests.end() && ret == true; ++request)
      {
         //do the request
         try
         {
            shared_ptr<http::response> response = http::postForm(request->first, request->second, connection, headers);
            if(response->statusCode != 200)
            {
               LOG_ERROR(boost::format("Failed to PUT tile: %1% (status=%2%)") % request->first % response->statusCode);
               ret = false;
            }
         }
         catch(std::runtime_error e)
         {
            LOG_ERROR(boost::format("Runtime error serially PUTTING tile: %1%") % e.what());
            return false;
         }
      }

      //return whether we were successful
      return ret;
   }

   bool http_storage::get_meta(const tile_protocol &tile, string &metatile) const
   {
      //put extra stuff in the http header
      vector<string> headers = this->make_headers(NULL, (char*)NULL);
      //get the requests
      vector<string> requests = make_get_requests(tile);
      //place to keep the responses
      vector<shared_ptr<http::response> > responses;

#ifdef RENDERMQ_DEBUG
      LOG_FINER(boost::format("Getting metatile: %1%") % tile);
      bt::ptime start = bt::microsec_clock::local_time();
#endif

      //if we aren't using concurrency
      bool ret = (concurrency < 2 ? get_meta_serial(requests, headers, responses) : get_meta_parallel(requests, headers, responses));

#ifdef RENDERMQ_DEBUG
      bt::ptime end = bt::microsec_clock::local_time();
      LOG_FINER(boost::format("Finished getting metatile: %1% in %2% secs") % tile % (end - start));
#endif

      //if we failed for any reason
      if(ret == false)
      {
         LOG_FINER("Metatile get failed.");
      }

      //make the metatile out of it
      make_metatile(tile, responses, metatile);

      //return whether we were successful
      return ret;
   }

   bool http_storage::get_meta_parallel(const vector<string>& requests, const vector<string>& headers, vector<shared_ptr<http::response> >& responses) const
   {
      //do the requests
      try
      {
         responses = http::multiGet(requests, concurrency, connection, headers);
      }
      catch(std::runtime_error e)
      {
         LOG_ERROR(boost::format("Runtime error asynchronously GETTING tile: %1%") % e.what());
         return false;
      }

      //check the responses of each request
      bool ret = true;
      vector<string>::const_iterator request = requests.begin();
      for (vector<shared_ptr<http::response> >::const_iterator response = responses.begin(); response != responses.end(); ++response, ++request)
      {
         //if we didn't get a 200 or the tile is dirty
         if((*response)->statusCode != 200 || (*response)->timeStamp == INVALID_TIMESTAMP)
         {
            //should we really log this?
            LOG_FINER(boost::format("Failed to GET tile: %1% (status=%2%, last-mod=%3%)") % *request % (*response)->statusCode % (*response)->timeStamp);
            ret = false;
         }
      }

      return ret;
   }

   bool http_storage::get_meta_serial(const vector<string>& requests, const vector<string>& headers, vector<shared_ptr<http::response> >& responses) const
   {
      //do each request and check the response
      bool ret = true;
      for(vector<string>::const_iterator request = requests.begin(); request != requests.end() && ret == true; ++request)
      {
         //do the request
         try
         {
            shared_ptr<http::response> response = http::get(*request, connection, headers);
            //if we didn't get a 200 or the tile is dirty
            if(response->statusCode != 200 || response->timeStamp == INVALID_TIMESTAMP)
            {
               //should we really log this?
               LOG_FINER(boost::format("Failed to GET tile: %1% (status=%2%, last-mod=%3%)") % *request % response->statusCode % response->timeStamp);
               ret = false;
            }
            //keep it
            responses.push_back(response);
         }
         catch(std::runtime_error e)
         {
            LOG_FINER(boost::format("Runtime error synchronously GETTING tile: %1%") % e.what());
            return false;
         }
      }

      //return whether we were successful
      return ret;
   }

   void http_storage::make_metatile(const tile_protocol &tile, const vector<shared_ptr<http::response> >& responses, string& metatile) const
   {
      //place to keep the formats
      vector<protoFmt> formats = rendermq::get_formats_vec(tile.format);
      //place to keep sizes
      vector<int> sizes(formats.size() * METATILE * METATILE, 0);
      vector<int>::iterator tileSize = sizes.begin();
      unsigned int metaSize = 0;
      int dim = get_meta_dimensions(tile.z);
      int size = get_tile_count_in_meta(tile.z);

      //something isn't right!
      if(size * formats.size() != responses.size())
         LOG_ERROR(boost::format("Failed to reconstruct metatile, expected %1% responses got %2%") % (size * formats.size()) % responses.size());

      //keep the size of each tile in the metatile response
      vector<shared_ptr<http::response> >::const_iterator response = responses.begin();
      for(size_t f = 0; f < formats.size(); f++)
      {
         //rows
         for(int y = 0; y < METATILE; y++)
         {
            //columns
            for(int x = 0; x < METATILE; x++)
            {
               //if there should be a response for this tile
               if(x < dim && y < dim)
               {
                  //keep track of the tile size, cast isn't a problem for tiles smaller than 4gig :o)
                  *tileSize = int((*response)->body.length());
                  metaSize += *tileSize;
                  //printf("%d %d %d -> %s\n", tile.z, x + tile.x, y + tile.y, (*response)->body.c_str());
                  //next response
                  response++;
               }
               //next size in the table
               tileSize++;
            }
         }
      }

      //get rid of anything that is in there
      metatile.clear();
      //put the header there
      pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);
      metatile = write_headers(coord.first, coord.second, tile.z, formats, sizes);
      //make space for the tiles so that we don't have to do multiple allocations
      metatile.reserve(metatile.length() + metaSize + 1);
      //put all the tiles in there. can do them all in a row because blank ones have no size
      for(response = responses.begin(); response != responses.end(); ++response)
      {
         metatile += (*response)->body;
      }
      //end the data
      metatile += "\0";
   }

   vector<string> http_storage::make_headers(const std::time_t* last_modified, ...) const
   {
      vector<string> headers;

      //add the last modified header
      if(last_modified != NULL)
      {
         std::stringstream last_mod_str;
         last_mod_str << "Last-Modified: ";
         date_formatter(last_mod_str, *last_modified);
         headers.push_back(last_mod_str.str());
      }

      //remove the Expect continue header, some servers don't like it
      headers.push_back("Expect:");

      //add additional headers
      va_list list;
      va_start(list, last_modified);
      char* header;
      while((header = va_arg(list, char*)) != NULL)
         headers.push_back(header);
      va_end(list);

      //hand back the list of headers
      return headers;
   }
}

