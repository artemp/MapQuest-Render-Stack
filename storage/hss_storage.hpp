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

#ifndef RENDERMQ_HSS_STORAGE_HPP
#define RENDERMQ_HSS_STORAGE_HPP

#include <boost/format.hpp>
#include "http_storage.hpp"

namespace rendermq
{

   class hss_storage: public http_storage
   {
      public:
         hss_storage(const string& host, const unsigned int& port, const string& app_name, const string &prefix, const string& version,
            boost::optional<string> atomics_host = boost::optional<string>(), const bool& persistent = false, const int& concurrency = 1);
         virtual ~hss_storage();
         //get a single tile in a single format
         virtual boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;
         //expires a tile by setting last modified to invalid (easiest way to expire them)
         virtual bool expire(const tile_protocol &tile) const;

      protected:
         //constructs a url for a single tile
         virtual const string form_url(const int& x, const int& y, const int& z, const string& style, const protoFmt& format,
            const bool& isPost = false, const string &command = "storage") const;
         //constructs the host portion of the url
         virtual const string make_host(const string& style, const string &command, const bool& isPost) const;
         //constructs the hex id portion of the url
         virtual const string make_id(const int& x, const int& y, const int& z, const protoFmt& format) const;
         //generate the requests for the put
         virtual vector<pair<string, vector<http::part> > > make_put_requests(const tile_protocol &tile,const string &metatile) const;
         //generate the requests for the get
         virtual vector<string> make_get_requests(const tile_protocol &tile) const;

         //hss specific configuration parameters
         const string host;
         const unsigned int port;
         const string app_name;
         const string prefix;
         const string version;

         // optional location of atomics database-over-HTTP interface
         boost::optional<string> m_atomics_host;
   };

}

#endif // RENDERMQ_HSS_STORAGE_HPP
