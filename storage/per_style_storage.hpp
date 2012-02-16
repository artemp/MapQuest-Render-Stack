/*------------------------------------------------------------------------------
 *
 * Selects which storage to use based on the style parameter of the
 * object being stored or retrieved.
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

#ifndef RENDERMQ_PER_STYLE_STORAGE_HPP
#define RENDERMQ_PER_STYLE_STORAGE_HPP

#include <string>
#include <ctime>
#include <list>
#include <boost/shared_ptr.hpp>
#include "tile_storage.hpp"

namespace rendermq 
{

/* Uses the 'style' parameter of the object being stored or retrieved
 * to select a backend.
 *
 * This allows different styles to be stored in different places, or
 * accessed with different parameters. This can be useful when
 * different tile sets have different requirements or livenesses, for
 * example; terrain tiles are updated far less frequently than NavTeq
 * tiles, which are in turn less frequently updated than OSM tiles.
 */
class per_style_storage 
   : public tile_storage 
{
public:
   // the type of mapping of style name to storage instance used.
   typedef std::map<std::string, boost::shared_ptr<tile_storage> > map_of_storage_t;

   // create a per-style storage from existing storage objects and a
   // default storage object to be used when none of the style names
   // match an incoming object.
   per_style_storage(map_of_storage_t storages, boost::shared_ptr<tile_storage> deflt);
   ~per_style_storage();

   // see if the style is mentioned in the map, else use the default,
   // and proxy this request to the appropriate storage object.
   boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;
   bool get_meta(const tile_protocol &, std::string &) const;
   bool put_meta(const tile_protocol &tile, const std::string &buf) const;
   bool expire(const tile_protocol &tile) const;

private:

   // maps style name into a storage object to provide per-style
   // overrides for the storage behaviour.
   map_of_storage_t m_storages;

   // default storage object, which is used when the style name of the
   // tile doesn't match any in the above map.
   boost::shared_ptr<tile_storage> m_default_storage;
};

}

#endif // RENDERMQ_PER_STYLE_STORAGE_HPP
