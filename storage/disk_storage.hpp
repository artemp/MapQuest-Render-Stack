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

#ifndef RENDERMQ_DISK_STORAGE_HPP
#define RENDERMQ_DISK_STORAGE_HPP

#include <boost/array.hpp>
#include <string>
#include <ctime>
#include "tile_storage.hpp"

namespace rendermq {

class disk_storage : public tile_storage {
public:
  typedef boost::array<unsigned char,1024*1024> tile_data;

  class handle : public tile_storage::handle {
  public:
    handle(std::time_t, size_t, const disk_storage &);
    virtual ~handle();
    virtual bool exists() const;
    virtual std::time_t last_modified() const;
    virtual bool data(std::string &) const;
    virtual bool expired() const;
  private:
    std::time_t timestamp;
    size_t size;
    const disk_storage &parent;
  };
  friend class handle;

  disk_storage(std::string const& dir);
  ~disk_storage();
  boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;
  bool get_meta(const tile_protocol &, std::string &) const;
  bool put_meta(const tile_protocol &tile, const std::string &buf) const;
  bool expire(const tile_protocol &tile) const;

private:

  std::string dir_;

  mutable bool data_locked;
  mutable tile_data data_cache;
};

}

#endif // RENDERMQ_DISK_STORAGE_HPP
