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

#ifndef TILE_BROKER_IMPL_HPP
#define TILE_BROKER_IMPL_HPP

#include <zmq.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/scoped_ptr.hpp>

namespace rendermq {

/* implementation of broker. this is extracted from the main CPP file so that it
 * can be reused internally within the tests.
 */
class broker_impl {
public:
  // set up the broker
  broker_impl(const boost::property_tree::ptree &config, 
              const std::string &broker_name, 
              zmq::context_t &ctx);

  ~broker_impl();
  
  // make the broker go into an event loop
  void operator()();

private:
  struct pimpl;
  boost::scoped_ptr<pimpl> impl;
};

} // namespace rendermq

#endif /* TILE_BROKER_IMPL_HPP */
