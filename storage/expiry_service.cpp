/*------------------------------------------------------------------------------
 *
 *  Interface to a service which can answer whether or not a metatile
 *  has been expired or not.
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

#include "expiry_service.hpp"
#include <boost/bind.hpp>
#include "../zstream_pbuf.hpp"

#define REQUEST_TIMEOUT (1000000)
#define SETTLE_TIMEOUT (2000000)

using std::string;
using boost::shared_ptr;
namespace pt = boost::property_tree;
namespace manip = zstream::manip;

namespace rendermq {

expiry_service::expiry_service(zmq::context_t &ctx, const pt::ptree &conf) 
   : m_context(ctx),
     m_req_ptr(new zstream::socket::req(m_context)),
     m_server_number(0)
{
   m_servers[0] = conf.get<string>("primary");
   m_servers[1] = conf.get<string>("backup");

   m_req_ptr->connect(m_servers[m_server_number]);
}

expiry_service::~expiry_service()
{
}

bool 
expiry_service::request_with_failover(boost::function<void (zstream::socket::req &)> sender) const
{
   sender(*m_req_ptr);

   bool response = false, waiting_for_reply = true;
   while (waiting_for_reply) 
   {
      zmq::pollitem_t items[] = { { m_req_ptr->socket(), 0, ZMQ_POLLIN, 0 } };
      
      try
      {
         zmq::poll(items, 1, REQUEST_TIMEOUT);
      }
      catch(const std::exception &ex)
      {
         std::cerr << "WARNING: got exception in zmq::poll: " << ex.what() << std::endl;
         continue;
      }

      if (items[0].revents & ZMQ_POLLIN) 
      {
         uint32_t data = 0;
         (*m_req_ptr) >> data;
         response = data != 0;
         waiting_for_reply = false;
      }
      else
      {
         // failed to get a response from the server - need to 
         // switch to the backup server...

         // kill the existing connection
         m_req_ptr.reset();

         // switch and settle to avoid the thundering herds...
         m_server_number = (m_server_number + 1) % 2;
         usleep(SETTLE_TIMEOUT);

         // connect to the new server
         m_req_ptr.reset(new zstream::socket::req(m_context));
         m_req_ptr->connect(m_servers[m_server_number]);

         // resend request
         sender(*m_req_ptr);
      }
   }

   return response;
}

namespace
{

void send_get_request(zstream::socket::req &socket,
                      const tile_protocol &meta_tile) 
{
   socket << meta_tile;
}

void send_set_request(zstream::socket::req &socket,
                      const tile_protocol &meta_tile,
                      uint32_t value) 
{
   socket << manip::more << meta_tile << value;
}

} // anonymous namespace

bool 
expiry_service::is_expired(const tile_protocol &meta_tile) const 
{
   return request_with_failover(boost::bind(send_get_request, _1, meta_tile));
}

bool 
expiry_service::set_expired(const tile_protocol &meta_tile, bool status) 
{
   return request_with_failover(boost::bind(send_set_request, _1, meta_tile, status));
}

} // namespace rendermq

