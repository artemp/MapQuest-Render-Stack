/*------------------------------------------------------------------------------
 *
 *  Null (as in /dev/null) "storage" layer - it doesn't actually
 *  store anything. 
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

#ifndef RENDERMQ_NULL_STORAGE_HPP
#define RENDERMQ_NULL_STORAGE_HPP

#include <string>
#include <ctime>
#include "tile_storage.hpp"

namespace rendermq {

/* a /dev/null storage device. throws away everything that's passed to
 * it and doesn't ever claim to have a tile. this is pretty useful for
 * testing that the system still works after a catastrophic storage
 * device failure.
 */
class null_storage : public tile_storage {
public:
  null_storage();
  virtual ~null_storage();

  boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;
  bool get_meta(const tile_protocol &tile, std::string &) const;
  bool put_meta(const tile_protocol &tile, const std::string &buf) const;
  bool expire(const tile_protocol &tile) const;
};

}

#endif // RENDERMQ_NULL_STORAGE_HPP
