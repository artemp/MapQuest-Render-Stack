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

#include "null_storage.hpp"
#include "null_handle.hpp"

using std::string;
using std::time_t;
using boost::shared_ptr;

namespace rendermq {

namespace {

tile_storage * create_null_storage(boost::property_tree::ptree const& pt,
                                   boost::optional<zmq::context_t &> ctx) {
  return new null_storage();
}

const bool registered = register_tile_storage("null", create_null_storage);

} // anonymous namespace

null_storage::null_storage() {
}

null_storage::~null_storage() {
}

shared_ptr<tile_storage::handle> 
null_storage::get(const tile_protocol &tile) const {
  return shared_ptr<tile_storage::handle>(new null_handle());
}

bool 
null_storage::get_meta(const tile_protocol &, std::string &) const {
  return false;
}

bool 
null_storage::put_meta(const tile_protocol &tile, const std::string &buf) const {
  return true;
}

bool 
null_storage::expire(const tile_protocol &tile) const {
  return true;
}

}

