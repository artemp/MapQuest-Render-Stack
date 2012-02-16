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

#include "expiry_overlay.hpp"
#include "tile_storage.hpp"
#include <boost/foreach.hpp>

using boost::shared_ptr;
using std::string;
namespace pt = boost::property_tree;

namespace 
{

/* simple overlay on top of an existing handle which replaces 
 * only the expiry information.
 */
class overlay_handle
   : public rendermq::tile_storage::handle
{
public:
   overlay_handle(shared_ptr<rendermq::tile_storage::handle> h, bool e)
      : m_handle(h), m_expired(e) {}
   ~overlay_handle() {}
   
   bool exists() const { return m_handle->exists(); }
   std::time_t last_modified() const { return m_handle->last_modified(); }
   bool data(std::string &str) const { return m_handle->data(str); }
   bool expired() const { return m_expired; }

private:
   shared_ptr<rendermq::tile_storage::handle> m_handle;
   bool m_expired;
};

rendermq::tile_storage *create_expiry_overlay(const pt::ptree &conf, 
                                              boost::optional<zmq::context_t &> ctx)
{
   using rendermq::expiry_overlay;
   using rendermq::expiry_service;
   using rendermq::tile_storage;

   if (!ctx) 
   {
      throw std::runtime_error("0MQ context not provided, but is required.");
   }

   string storage = conf.get<string>("storage");

   // create a new property tree for the sub storage to use.
   pt::ptree sub_conf = conf.get_child(storage, pt::ptree());

   // the substring that we want to match is the name, plus a dot
   // as a separator - the rest is the key that the sub storage 
   // instance will be looking for.
   storage.append(".");

   BOOST_FOREACH(pt::ptree::value_type entry, conf) 
   {
      if (entry.first.compare(0, storage.size(), storage) == 0)
      {
         // use semi-colon as a path separator we're not likely to 
         // see, since that is the comment character for INI files.
         boost::property_tree::path_of<string>::type p(entry.first, ';');
         
         sub_conf.put(entry.first.substr(storage.size()), conf.get<string>(p));
      }
   }

   // attempt to create the storage
   tile_storage *ptr = rendermq::get_tile_storage(sub_conf, ctx);

   if (ptr == NULL)
   {
      throw std::runtime_error("Failed to create storage for expiry overlay.");
   }

   expiry_overlay *overlay = 
      new expiry_overlay(shared_ptr<tile_storage>(ptr),
                         shared_ptr<expiry_service>(new expiry_service(ctx.get(), conf)));

   return overlay;
}

const bool registered = register_tile_storage("expiry_overlay", create_expiry_overlay);

} // anonymous namespace

namespace rendermq 
{

expiry_overlay::expiry_overlay(shared_ptr<tile_storage> storage,
                               shared_ptr<expiry_service> expiry)
   : m_storage(storage), m_expiry(expiry)
{
}

expiry_overlay::~expiry_overlay()
{
}

shared_ptr<tile_storage::handle> 
expiry_overlay::get(const tile_protocol &tile) const
{
   bool expired = m_expiry->is_expired(tile);
   return shared_ptr<tile_storage::handle>(new overlay_handle(m_storage->get(tile), expired));
}

bool 
expiry_overlay::get_meta(const tile_protocol &tile, string &data) const
{
   return m_storage->get_meta(tile, data);
}

bool 
expiry_overlay::put_meta(const tile_protocol &tile, const string &buf) const 
{
   m_expiry->set_expired(tile, false);
   return m_storage->put_meta(tile, buf);
}

bool 
expiry_overlay::expire(const tile_protocol &tile) const 
{
   return m_expiry->set_expired(tile, true);
}

} // namespace rendermq
