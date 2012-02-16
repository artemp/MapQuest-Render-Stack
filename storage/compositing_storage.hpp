/*------------------------------------------------------------------------------
 *
 * Virtual storage which composites tiles from two other stores and
 * presents a view that these composite images are natively stored.
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2011 Mapquest, Inc.  All Rights reserved.
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

#ifndef RENDERMQ_COMPOSITING_STORAGE_HPP
#define RENDERMQ_COMPOSITING_STORAGE_HPP

#include <string>
#include <ctime>
#include <list>
#include <boost/shared_ptr.hpp>
#include "tile_storage.hpp"

namespace rendermq 
{

/* Makes it appear as if there is a store containing tiles which are
 * really composited on-the-fly from two different storage systems.
 *
 * This means that this storage object is unable to write new results.
 * It can, however, deal with expiries via configurable behaviour to
 * expire one or other (or both) of the input tiles.
 */
class compositing_storage 
   : public tile_storage 
{
public:
   // create a composite storage given a ...
   compositing_storage(boost::shared_ptr<tile_storage> under,
                       boost::shared_ptr<tile_storage> over,
                       const boost::property_tree::ptree &config);
   ~compositing_storage();

   // gets tiles from both storages. a failure of either causes the
   // failure of this storage. tiles from the two storages are 
   // composited and the result re-encoded before being returned.
   // last-modified handling is conservative: the time for the 
   // returned tile is the youngest of the two inputs.
   boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;
   bool get_meta(const tile_protocol &, std::string &) const;

   // always fails - there is no way to store to this "storage" type
   bool put_meta(const tile_protocol &tile, const std::string &buf) const;

   // can expire one, both or neither of the input storages.
   bool expire(const tile_protocol &tile) const;

private:

   // storage sources for the under (background) and over 
   // (foreground) tiles.
   boost::shared_ptr<tile_storage> m_under_storage, m_over_storage;

   // configuration for the re-encoding of the output.
   boost::property_tree::ptree m_config;

   // optional changes of style for the under and over 
   // storages.
   boost::optional<std::string> m_under_style, m_over_style;

   // the respective formats to request from the under and
   // over storage.
   protoFmt m_generate_format, m_under_format, m_over_format;

   // checks if the formats requested are a strict subset
   // of those available.
   bool can_generate_formats(protoFmt formats) const;
};

}

#endif // RENDERMQ_COMPOSITING_STORAGE_HPP
