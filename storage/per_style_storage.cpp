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

#include <vector>
#include <string>
#include <list>
#include <stdexcept>

#include "per_style_storage.hpp"
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

using boost::shared_ptr;
using std::string;
using std::vector;
using std::string;
using std::list;
namespace bt = boost::property_tree;

namespace 
{
boost::shared_ptr<rendermq::tile_storage> 
make_subtree(const bt::ptree &pt, boost::optional<zmq::context_t &> ctx, const string &name)
{
   using rendermq::tile_storage;

   // create a new property tree for the sub storage to use.
   bt::ptree sub_pt = pt.get_child(name, bt::ptree());
   
   // the substring that we want to match is the name, plus a dot
   // as a separator - the rest is the key that the sub storage 
   // instance will be looking for.
   string prefix = name + ".";
   
   BOOST_FOREACH(bt::ptree::value_type entry, pt) 
   {
      if (entry.first.compare(0, prefix.size(), prefix) == 0)
      {
         // use semi-colon as a path separator we're not likely to 
         // see, since that is the comment character for INI files.
         boost::property_tree::path_of<string>::type p(entry.first, ';');
         
         sub_pt.put(entry.first.substr(prefix.size()), pt.get<string>(p));
      }
   }
   
   // attempt to create the storage
   tile_storage *ptr = rendermq::get_tile_storage(sub_pt, ctx);
   
   if (ptr == NULL)
   {
      throw std::runtime_error("Failed to create storage item within per-style.");
   }
   else
   {
      return shared_ptr<tile_storage>(ptr);
   }
}

rendermq::tile_storage *create_per_style_storage(const bt::ptree &pt,
                                                 boost::optional<zmq::context_t &> ctx)
{
   // NOTE: the way we do paths with dots in them is going to cause
   // a bunch of trouble if we ever decide we want to start nesting/
   using rendermq::tile_storage;

   vector<string> splits;
   string style_names = pt.get<string>("styles");
   rendermq::per_style_storage::map_of_storage_t styles;

   boost::split(splits, style_names, boost::is_any_of(", "), boost::token_compress_on);

   // for each style specified, look for a child tree with that name, 
   // or keys prefixed with the name and a dot '.', which is used to
   // simulate hierarchical configs in the INI format, which doesn't
   // support it directly.
   BOOST_FOREACH(string name, splits)
   {
      boost::shared_ptr<tile_storage> storage = make_subtree(pt, ctx, name);
      styles.insert(make_pair(name, storage));
   }
   boost::shared_ptr<tile_storage> default_storage = make_subtree(pt, ctx, "default");

   return new rendermq::per_style_storage(styles, default_storage);
}

const bool registered = register_tile_storage("per_style", create_per_style_storage);

} // anonymous namespace

namespace rendermq 
{

per_style_storage::per_style_storage(map_of_storage_t storages,
                                     boost::shared_ptr<tile_storage> deflt) 
   : m_storages(storages), m_default_storage(deflt)
{
}

per_style_storage::~per_style_storage() 
{
}

shared_ptr<tile_storage::handle> 
per_style_storage::get(const tile_protocol &tile) const 
{
   map_of_storage_t::const_iterator itr = m_storages.find(tile.style);
   if (itr == m_storages.end()) 
   {
      return m_default_storage->get(tile);
   }
   else
   {
      return itr->second->get(tile);
   }
}

bool 
per_style_storage::get_meta(const tile_protocol &tile, std::string &data) const {
   map_of_storage_t::const_iterator itr = m_storages.find(tile.style);
   if (itr == m_storages.end()) 
   {
      return m_default_storage->get_meta(tile, data);
   }
   else
   {
      return itr->second->get_meta(tile, data);
   }
}

bool 
per_style_storage::put_meta(const tile_protocol &tile, const std::string &buf) const 
{
   map_of_storage_t::const_iterator itr = m_storages.find(tile.style);
   if (itr == m_storages.end()) 
   {
      return m_default_storage->put_meta(tile, buf);
   }
   else
   {
      return itr->second->put_meta(tile, buf);
   }
}   

bool 
per_style_storage::expire(const tile_protocol &tile) const 
{
   map_of_storage_t::const_iterator itr = m_storages.find(tile.style);
   if (itr == m_storages.end()) 
   {
      return m_default_storage->expire(tile);
   }
   else
   {
      return itr->second->expire(tile);
   }
}

} // namespace rendermq

