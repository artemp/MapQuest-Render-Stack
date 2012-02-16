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

#ifndef TILE_STORAGE_HPP
#define TILE_STORAGE_HPP

#include "../tile_protocol.hpp"
#include <mapnik/utils.hpp>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/property_tree/ptree.hpp>
#include <string>
#include <map>

namespace rendermq
{
/* interface for all tile storage implementations. the implementations
 * themselves are only available through the get_tile_storage function,
 * as the tile_storage_factory is responsible for instantiating and 
 * looking them up.
 */
struct tile_storage 
  : private boost::noncopyable {
  /* an interface for querying information about a tile. since in many
   * implementations of tile storage the act of retrieving a tile may be
   * quite expensive, the implementation may choose to buffer some
   * information here.
   */
  struct handle 
    : private boost::noncopyable {
    // whether the tile exists in storage
    virtual bool exists() const = 0;

    // the time that the tile was last modified, according to the 
    // storage layer.
    virtual std::time_t last_modified() const = 0;

    // copy the data of this *tile* into the provided string, returning
    // whether the copy was succesful. note that this is only this tile
    // and not the whole metatile. for that, use get_meta().
    virtual bool data(std::string &) const = 0;

    // whether the tile has been marked as dirty, or expired. the tile
    // might be present and, under some circumstances, it might still be
    // worth serving it to the client.
    virtual bool expired() const = 0;

    virtual ~handle();
  };
  
  /* gets a handle which can be used to retrieve data about a given tile.
   */
  virtual boost::shared_ptr<handle> get(const tile_protocol &tile) const = 0;

  /* reads a full, encoded meta tile into the given string. returns whether
   * the copy was successful or not. note that this *may* be less efficient
   * than calling get() if all you need is a single tile.
   */
  virtual bool get_meta(const tile_protocol &, std::string &) const = 0;

  /* saves a metatile, as given in the buf parameter, to storage.
   */
  virtual bool put_meta(const tile_protocol &tile, const std::string &buf) const = 0;

  /* mark a whole meta tile as expired, such that retrieving any tile within
   * this metatile will be present, but have the expired flag set.
   */
  virtual bool expire(const tile_protocol &tile) const = 0;

  virtual ~tile_storage();
};

// factory function
typedef tile_storage * (* storage_creator)(boost::property_tree::ptree const&,
                                           boost::optional<zmq::context_t &>);

/* concrete factory implementation.
 * note that this is used internally, and does not need to be called from 
 * outside the implementation of tile_storage. it really doesn't need to be
 * in this header at all.
 */
class tile_storage_factory : public mapnik::singleton<tile_storage_factory, mapnik::CreateStatic>,
                             private boost::noncopyable
{
public:
   friend class mapnik::CreateStatic<tile_storage_factory>;
   bool add(std::string const& type, storage_creator func);
   bool remove(std::string const& type);
   tile_storage * create(boost::property_tree::ptree const& pt,
                         boost::optional<zmq::context_t &>);
private:
    std::map<std::string,storage_creator> cont;
};

/* called from storage implementations to register them with the singleton
 * factory instance.
 */
bool register_tile_storage(std::string const& type, storage_creator func);

/* factory constructor. returns a pointer to a new object, constructed with
 * the parameters provided from the configuration file. the caller owns the
 * returned object.
 */
tile_storage * get_tile_storage(boost::property_tree::ptree const& params,
                                boost::optional<zmq::context_t &> ctx = boost::optional<zmq::context_t &>());

}

#endif // TILE_STORAGE_HPP
