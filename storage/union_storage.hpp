/*------------------------------------------------------------------------------
 *
 *  A "union" of other storages, allowing easy (or easier, at least)
 *  migration between storage backends.
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

#ifndef RENDERMQ_UNION_STORAGE_HPP
#define RENDERMQ_UNION_STORAGE_HPP

#include <string>
#include <ctime>
#include <list>
#include <boost/shared_ptr.hpp>
#include "tile_storage.hpp"

namespace rendermq 
{

/* creates a "union" of other storages, so that they can be
 * used together to create an appearance of a single storage
 * object. 
 *
 * in this way it's possible to add an extra storage service
 * without disruption to the main service, and to transition
 * over gradually, having the cache of the secondary storage
 * filled as jobs are requested due to expiry from the main
 * storage.
 */
class union_storage 
   : public tile_storage 
{
public:
   // the collection of tile storage objects "unioned" in 
   // this implementation.
   typedef std::list<boost::shared_ptr<tile_storage> > list_of_storage_t;

   // create a union storage from existing storage objects
   union_storage(list_of_storage_t storages);
   ~union_storage();

   // get the tile from the first storage in the list which
   // claims to have it.
   boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;

   // attempt to get the meta tile from the first storage
   // which claims to have it.
   bool get_meta(const tile_protocol &, std::string &) const;

   // put the meta tile to *all* unioned storages.
   bool put_meta(const tile_protocol &tile, const std::string &buf) const;

   // expire the tile from *all* unioned storages.
   bool expire(const tile_protocol &tile) const;

private:
   
   list_of_storage_t m_storages;
};

}

#endif // RENDERMQ_UNION_STORAGE_HPP
