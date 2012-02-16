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

#ifndef RENDERMQ_EXPIRY_SERVICE_HPP
#define RENDERMQ_EXPIRY_SERVICE_HPP

#include "../tile_protocol.hpp"
#include "../zstream.hpp"
#include <zmq.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace rendermq 
{

/* the expiry service knows about the state of all the 
 * metatiles in the system, and is able to answer queries
 * about the expiry status of any metatile.
 *
 * this can be used to separate the expiry information 
 * from the storage of the tile, which helps with systems
 * like HSS, where, due to the replicated nature of the
 * files, updating the file metadata seems to take a very
 * long time.
 *
 * this could also be useful for disk storage, as it means
 * that updating the expiry information for a very large
 * number of metatiles can be done very quickly.
 */
class expiry_service 
{
public:

   expiry_service(zmq::context_t &, const boost::property_tree::ptree &);
   ~expiry_service();

   // returns whether or not a given metatile is expired.
   bool is_expired(const tile_protocol &) const;

   // sets the expired flag information to be the same as
   // the boolean variable passed in. returns whether or
   // not the operation succeeded.
   bool set_expired(const tile_protocol &, bool);

private:
   // context for zeromq communications
   zmq::context_t &m_context;

   // request socket to the expiry server. this is stored
   // as a pointer since in the case of failure we need to
   // disconnect and reconned it.
   mutable boost::shared_ptr<zstream::socket::req> m_req_ptr;

   // current server number
   mutable int m_server_number;

   // primary and secondary server locations
   std::string m_servers[2];

   // send a request which fails over and retries to the
   // backup server if it doesn't get a response. the 
   // functor argument is the bit that sends the request.
   bool request_with_failover(boost::function<void (zstream::socket::req &)>) const;
};

}

#endif // RENDERMQ_EXPIRY_SERVICE_HPP
