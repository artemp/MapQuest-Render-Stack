/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: artem@mapnik-consulting.com
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

#ifndef HTTP_REPLY_HPP
#define HTTP_REPLY_HPP

#include <zmq.hpp>
#include <string>

#include "http_date_formatter.hpp"

namespace rendermq
{

void send_reply(zmq::socket_t & socket, 
                std::string const& uuid, 
                std::string const& id , 
                int status, 
                std::string const& output);

void send_404(zmq::socket_t & socket,
              std::string const& uuid, 
              std::string const& id
   );

void send_304(zmq::socket_t & socket, 
              std::string const& uuid, 
              std::string const& id,
              std::time_t date, 
              http_date_formatter const& formatter,
              const std::string &mime_type);

// send the client a message indicating server error. currently used when
// the worker returns an error to the handler, and there's no fallback.
void send_500(zmq::socket_t &socket,
              const std::string &uuid,
              const std::string &id);

// when the queue length threshold is greater than the maximum allowed 
// then return this, "service overloaded" message to inform the client
// to try again later.
void send_503(zmq::socket_t &socket,
              const std::string &uuid,
              const std::string &id);


// sends a tile, along with Last-Modified and cache-related headers.
void send_tile(zmq::socket_t & socket, 
               http_date_formatter const& frmt,
               std::string const& uuid, 
               std::string const& id ,
               unsigned max_age , 
               std::time_t last_modified, 
               std::time_t expire_time,
               std::string const& data,
               const std::string &mime_type);

// sends a tile, but omits the Last-Modified and cache-related 
// headers.
void send_tile(zmq::socket_t & socket, 
               std::string const& uuid, 
               std::string const& id ,
               std::string const& data,
               const std::string &mime_type);

// when the queue length threshold is greater than a "satifiability" 
// threshold, and we don't have the tile, send this (but render in
// the background) so the client knows it's coming, but doesn't keep
// the connection open.
void send_202(zmq::socket_t &socket,
              const std::string &uuid,
              const std::string &id);

}

#endif // HTTP_REPLY_HPP
