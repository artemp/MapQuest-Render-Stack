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

#include <vector>
#include <string>
#include <list>
#include <stdexcept>

#include "union_storage.hpp"
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include "null_handle.hpp"

using boost::shared_ptr;
using std::string;
using std::vector;
using std::string;
using std::list;
namespace bt = boost::property_tree;

namespace 
{
rendermq::tile_storage *create_union_storage(const bt::ptree &pt,
                                             boost::optional<zmq::context_t &> ctx)
{
   // NOTE: the way we do paths with dots in them is going to cause
   // a bunch of trouble if we ever decide we want to start nesting
   // union storage types...

   vector<string> splits;
   string storage_names = pt.get<string>("storages");
   list<shared_ptr<rendermq::tile_storage> > storages;

   boost::split(splits, storage_names, boost::is_any_of(", "), boost::token_compress_on);

   // for each storage specified, look for a child tree with that name, 
   // or keys prefixed with the name and a dot '.', which is used to
   // simulate hierarchical configs in the INI format, which doesn't
   // support it directly.
   BOOST_FOREACH(string name, splits)
   {
      // create a new property tree for the sub storage to use.
      bt::ptree sub_pt = pt.get_child(name, bt::ptree());

      // the substring that we want to match is the name, plus a dot
      // as a separator - the rest is the key that the sub storage 
      // instance will be looking for.
      name.append(".");

      BOOST_FOREACH(bt::ptree::value_type entry, pt) 
      {
         if (entry.first.compare(0, name.size(), name) == 0)
         {
            // use semi-colon as a path separator we're not likely to 
            // see, since that is the comment character for INI files.
            boost::property_tree::path_of<string>::type p(entry.first, ';');

            sub_pt.put(entry.first.substr(name.size()), pt.get<string>(p));
         }
      }

      // attempt to create the storage
      rendermq::tile_storage *ptr = rendermq::get_tile_storage(sub_pt, ctx);

      if (ptr != NULL)
      {
         storages.push_back(shared_ptr<rendermq::tile_storage>(ptr));
      }
      else
      {
         throw std::runtime_error("Failed to create storage item within union.");
      }
   }

   return new rendermq::union_storage(storages);
}

const bool registered = register_tile_storage("union", create_union_storage);

} // anonymous namespace

namespace rendermq 
{

union_storage::union_storage(list_of_storage_t storages) 
   : m_storages(storages) 
{
}

union_storage::~union_storage() 
{
}

shared_ptr<tile_storage::handle> 
union_storage::get(const tile_protocol &tile) const 
{
   BOOST_FOREACH(shared_ptr<tile_storage> storage, m_storages) 
   {
      shared_ptr<tile_storage::handle> handle = storage->get(tile);
      if (handle->exists()) 
      {
         return handle;
      }
   }
   return shared_ptr<tile_storage::handle>(new null_handle());
}

bool 
union_storage::get_meta(const tile_protocol &tile, std::string &data) const {
   BOOST_FOREACH(shared_ptr<tile_storage> storage, m_storages) 
   {
      bool success = storage->get_meta(tile, data);
      if (success)
      {
         return success;
      }
   }
   return false;
}

bool 
union_storage::put_meta(const tile_protocol &tile, const std::string &buf) const 
{
   bool success = true;
   BOOST_FOREACH(shared_ptr<tile_storage> storage, m_storages) 
   {
      success &= storage->put_meta(tile, buf);
   }
   return success;
}   

bool 
union_storage::expire(const tile_protocol &tile) const 
{
   bool success = true;
   BOOST_FOREACH(shared_ptr<tile_storage> storage, m_storages) 
   {
      success &= storage->expire(tile);
   }
   return success;
}

} // namespace rendermq

