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

#include "meta_tile.hpp"
#include "disk_storage.hpp"
#include "../logging/logger.hpp"
#include "null_handle.hpp"

// stl
#include <iostream>
#include <fstream>
#include <vector>
// boost
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/format.hpp>

using std::string;
using std::time_t;
using std::runtime_error;
using std::cerr;
using std::pair;
using boost::shared_ptr;
namespace fs = boost::filesystem;

namespace rendermq {

namespace {

tile_storage * create_disk_storage(boost::property_tree::ptree const& pt,
                                   boost::optional<zmq::context_t &> ctx)
{
    boost::optional<string> tile_cache_dir = pt.get_optional<string>("tile_dir");
    if ( tile_cache_dir )
    {
        return new disk_storage(*tile_cache_dir);
    }
    return 0;
}

const bool registered = register_tile_storage("disk",create_disk_storage);

} // anonymous namespace

disk_storage::handle::handle(std::time_t t, size_t s, const disk_storage &p)
  : timestamp(t), size(s), parent(p) {
  parent.data_locked = true;
}

disk_storage::handle::~handle() {
  parent.data_locked = false;
}

bool 
disk_storage::handle::exists() const {
  return true;
}

std::time_t 
disk_storage::handle::last_modified() const {
  return timestamp;
}

bool
disk_storage::handle::expired() const {
  // on disk we consider something expired if it's timestamp is before
  // the planet_timestamp. but since we don't have that, we'll just take
  // the epoch.
  return last_modified() == 0;
}

bool
disk_storage::handle::data(string &output) const {
  output.assign((const char *)parent.data_cache.data(), size);
  return true;
}

disk_storage::disk_storage(string const& dir)
  : dir_(dir), data_locked(false)  {}

disk_storage::~disk_storage() {}

shared_ptr<tile_storage::handle> 
disk_storage::get(const tile_protocol &tile) const {
  if (data_locked) {
    throw runtime_error("Multiple use of disk_storage::data_cache not allowed.");
  }

  pair<string, int> foo = xyz_to_meta(dir_, tile.x, tile.y, tile.z, tile.style);
  try {
    fs::path p(foo.first);

    if (fs::exists(p)) {
      std::time_t t = fs::last_write_time(p);
      int ret = read_from_meta(dir_, tile.x, tile.y, tile.z, tile.style, 
                               data_cache.c_array(), data_cache.size(), 
                               tile.format);
      
      if (ret > 0) {
        return shared_ptr<tile_storage::handle>(new handle(t, ret, *this));
      }
    }

  } catch (const fs::filesystem_error &e) {
     LOG_ERROR(boost::format("Filesystem error: %1%") % e.what());
  }

  return shared_ptr<tile_storage::handle>(new null_handle());
}

bool 
disk_storage::get_meta(const tile_protocol &tile, std::string &data) const {
  pair<string, int> foo = xyz_to_meta(dir_, tile.x, tile.y, tile.z, tile.style);
  try {
    fs::path p(foo.first);

    if (fs::exists(p) && fs::is_regular_file(p)) {
      // if its expired we signal as such
      std::time_t t = fs::last_write_time(p);
      if(t == 0)
         return false;
      uintmax_t size = fs::file_size(p);
      fs::ifstream in(p);
      data.resize(size);
      // ooh, evil. cast away the const...
      in.read((char *)data.data(), size);
      return bool(in);
    }
  } catch (const fs::filesystem_error &e) {
     LOG_ERROR(boost::format("Filesystem error: %1%") % e.what());
  }

  return false;
}

bool 
disk_storage::put_meta(const tile_protocol &tile, const std::string &buf) const {
  pair<string, int> foo = xyz_to_meta(dir_, tile.x, tile.y, tile.z, tile.style);

  if (foo.second == 0) {
    fs::path tmp = fs::path(dir_) / fs::unique_path();

    try {
      fs::path p(foo.first);
      // create directory for metatile to go in.
      fs::create_directories(p.parent_path());

      // write first to temporary location
      {
        fs::ofstream out(tmp); 
        out << buf;
      }

      // now copy that file atomically into position
      fs::rename(tmp, p);

      return true;

    } catch (const fs::filesystem_error &e) {
       LOG_ERROR(boost::format("Filesystem error: %1%") % e.what());
    }

  } else {
#ifdef RENDERMQ_DEBUG
     LOG_ERROR("Attempt to save tile at non-metatile boundary.");
#endif
  }
  return false;    
}

bool 
disk_storage::expire(const tile_protocol &tile) const {
  pair<string, int> foo = xyz_to_meta(dir_, tile.x, tile.y, tile.z, tile.style);
  try {
    fs::path p(foo.first);
    
    if (fs::exists(p)) {
      // indicate that a tile has expired by setting its time to the 
      // unix epoch. it's not perfect, but things very rarely are.
      fs::last_write_time(p, std::time_t(0));

      return true;
    }

  } catch (const fs::filesystem_error &e) {
     LOG_ERROR(boost::format("Filesystem error: %1%") % e.what());
  }
  
  return false;
}

}

