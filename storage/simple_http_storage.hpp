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

#ifndef RENDERMQ_SIMPLE_HTTP_STORAGE_HPP
#define RENDERMQ_SIMPLE_HTTP_STORAGE_HPP

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

class simple_http_storage
   : public tile_storage
{
public:
   class handle
      : public tile_storage::handle
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

   simple_http_storage(const std::string &format);
   ~simple_http_storage();

   //get a single tile in a single format
   boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;

   //get each tile in each format and constructs a metatile from them
   bool get_meta(const tile_protocol &tile, string &metatile) const;

   // returns false - source is read-only
   bool put_meta(const tile_protocol &tile, const string &metatile) const;

   // returns false - source is read-only
   bool expire(const tile_protocol &tile) const;
   
protected:

   std::string m_format;
   boost::shared_ptr<CURL> m_connection;

   std::string make_url(const std::string &style, int z, int x, int y) const;
};

}

#endif // RENDERMQ_SIMPLE_HTTP_STORAGE_HPP
