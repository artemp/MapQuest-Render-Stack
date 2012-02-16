/*------------------------------------------------------------------------------
 *
 * Overlay this on another storage provider to use the expiry
 * service instead of storing expiry information with the
 * storage provider.
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

#ifndef RENDERMQ_EXPIRY_OVERLAY_HPP
#define RENDERMQ_EXPIRY_OVERLAY_HPP

#include <string>
#include <ctime>
#include <list>
#include <boost/shared_ptr.hpp>
#include "tile_storage.hpp"
#include "expiry_service.hpp"

namespace rendermq 
{

/* overlays the expiry service on top of an existing 
 * storage implementation, taking the responsibility of
 * handling expiry information away from that.
 *
 * the expiry service is, we assume, capable of dealing
 * with the expiry information more efficiently than 
 * the underlying storage.
 */
class expiry_overlay 
   : public tile_storage
{
public:

   expiry_overlay(boost::shared_ptr<tile_storage> storage,
                  boost::shared_ptr<expiry_service> expiry);
   ~expiry_overlay();

   // get the tile from the underlying storage object, but 
   // overlaid witht the expiry information from the expiry
   // service.
   boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;

   // get a metatile from the underlying storage.
   bool get_meta(const tile_protocol &, std::string &) const;

   // put the metatile to the storage and reset the expiry
   // formation for this metatile.
   bool put_meta(const tile_protocol &tile, const std::string &buf) const;

   // update the expiry service with this information.
   bool expire(const tile_protocol &tile) const;

private:
   boost::shared_ptr<tile_storage> m_storage;
   boost::shared_ptr<expiry_service> m_expiry;
};

}

#endif // RENDERMQ_EXPIRY_OVERLAY_HPP
