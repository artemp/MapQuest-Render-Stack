/*------------------------------------------------------------------------------
 *
 * Implementation of the expiry service, a redundant (2x) server
 * to handle queries as to whether a tile is expired or not.
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

#ifndef RENDERMQ_EXPIRY_SERVER_HPP
#define RENDERMQ_EXPIRY_SERVER_HPP

#include <string>
#include <ctime>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include "expiry_service.hpp"

namespace rendermq 
{

/* an implementation of the 'binary star' redundancy pattern
 * from the 0MQ guide (see http://zguide.zeromq.org).
 */
class expiry_server 
{
public:
   // state of the server in its finite state machine.
   enum state_t {
      state_primary = 1,
      state_backup  = 2,
      state_active  = 3,
      state_passive = 4
   };

   // events for state transition in the finite state machine.
   enum event_t {
      event_peer_primary   = 1,
      event_peer_backup    = 2,
      event_peer_active    = 3,
      event_peer_passive   = 4,
      event_client_request = 5
   };

   expiry_server(zmq::context_t &ctx,
                 state_t init_state,
                 const boost::property_tree::ptree &conf);
   ~expiry_server();

   // run the reactor
   void operator()();

private:
   // pimpl state machine object
   struct fsm;
   // pimpl expiry structure
   struct expiry_data;

   zmq::context_t &m_context;
   zstream::socket::xrep m_socket_frontend;
   zstream::socket::pub m_socket_statepub;
   zstream::socket::sub m_socket_statesub;

   boost::scoped_ptr<fsm> m_fsm;
   boost::scoped_ptr<expiry_data> m_expired;
};

}

#endif // RENDERMQ_EXPIRY_SERVER_HPP
