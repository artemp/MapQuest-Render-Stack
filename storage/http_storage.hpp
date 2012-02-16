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

#ifndef RENDERMQ_HTTP_STORAGE_HPP
#define RENDERMQ_HTTP_STORAGE_HPP

// the timestamp regarded as "invalid" or expired. here we use the
// epoch, midnight jan 1st 1970 GMT, as it's well before any tile
// that might reasonably be generated.
#define INVALID_TIMESTAMP (0l)

#include <string>
#include <vector>
#include <ctime>
#include "tile_storage.hpp"
#include "../http/http.hpp"
#include "../http/http_date_formatter.hpp"
#include "../logging/logger.hpp"

using boost::shared_ptr;
using std::string;
using std::vector;
using std::pair;

namespace rendermq
{

   class http_storage: public tile_storage
   {
      public:
         class handle: public tile_storage::handle
         {
            public:
               handle(const shared_ptr<http::response>& response);
               ~handle();
               virtual bool exists() const;
               virtual time_t last_modified() const;
               virtual bool data(string &) const;
               virtual bool expired() const;
            private:
               const shared_ptr<http::response> response;
         };
         friend class handle;

         http_storage(const bool& persistent = true, const int& concurrency = 1);
         virtual ~http_storage();
         //get a single tile in a single format
         virtual boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const = 0;
         //get each tile in each format and constructs a metatile from them
         virtual bool get_meta(const tile_protocol &tile, string &metatile) const;
         //put each tile in each format by deconstructing a metatile
         virtual bool put_meta(const tile_protocol &tile, const string &metatile) const;
         //expires a tile by setting last modified to invalid (easiest way to expire them)
         virtual bool expire(const tile_protocol &tile) const = 0;

      protected:

         //create http headers, last modified is optional by setting time_t to NULL
         //signify the end of the var args by setting the last argument to (char*)NULL
         virtual vector<string> make_headers(const time_t* last_modified, ...) const;

         //generate the requests for the put
         virtual vector<pair<string, vector<http::part> > > make_put_requests(const tile_protocol &tile,const string& metatile) const = 0;

         // puts tiles using a serial process
         virtual bool put_meta_serial(const vector<pair<string, vector<http::part> > >& requests, const vector<string>& headers) const;

         // puts tiles concurrently using a curl::multi pool
         virtual bool put_meta_parallel(const vector<pair<string, vector<http::part> > >& requests, const vector<string>& headers) const;

         //generate the requests for the get, THESE MUST BE IN METATILE ORDER ONE FORMAT AFTER THE NEXT (ie row/y column/x order)
         virtual vector<string> make_get_requests(const tile_protocol &tile) const = 0;

         // get tiles using a serial process
         virtual bool get_meta_serial(const vector<string>& requests, const vector<string>& headers, vector<shared_ptr<http::response> >& responses) const;

         // get tiles concurrently using a curl::multi pool
         virtual bool get_meta_parallel(const vector<string>& requests, const vector<string>& headers, vector<shared_ptr<http::response> >& responses) const;

         // given a bunch of tile responses build a metatile from them given a tile
         virtual void make_metatile(const tile_protocol &tile, const vector<shared_ptr<http::response> >& responses, string& metatile) const;

         //for last modified headers
         const http_date_formatter date_formatter;
         //keep a persistent connection
         const bool persistent;
         boost::shared_ptr<CURL> connection;
         // the number of outstanding connections to the HTTP storage
         const int concurrency;
   };

}

#endif // RENDERMQ_HTTP_STORAGE_HPP
