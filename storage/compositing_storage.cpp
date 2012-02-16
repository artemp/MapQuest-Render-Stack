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

#include <vector>
#include <string>
#include <list>
#include <stdexcept>

#include "compositing_storage.hpp"
#include "null_handle.hpp"
#include "../image/image.hpp"
#include "../logging/logger.hpp"

#include <boost/foreach.hpp>
#include <boost/format.hpp>

using boost::shared_ptr;
using std::string;
using std::vector;
using std::string;
using std::list;
namespace bt = boost::property_tree;

namespace 
{

// bit of code which actually does the compositing - taking
bool composite(string &under_data,  rendermq::protoFmt under_fmt,
               string &over_data,   rendermq::protoFmt over_fmt,
               string &result_data, rendermq::protoFmt result_fmt,
               const bt::ptree &config)
{
   using rendermq::image;

   shared_ptr<image> under_image = image::create(under_data, under_fmt);
   shared_ptr<image> over_image  = image::create(over_data,  over_fmt);

   // check for image creation failure - might not be the 
   // right format, mangled on wire, etc...
   if (!under_image || !over_image)
   {
      return false;
   }

   if ((under_image->width()  != over_image->width()) ||
       (under_image->height() != over_image->height())) 
   {
      LOG_ERROR(boost::format("Cannot composite images of different sizes: "
                              "under image (%1%x%2%), over image (%3%x%4%).")
                % under_image->width() % under_image->height()
                % over_image->width() % over_image->height());
      return false;
   }

   try 
   {
      under_image->merge(over_image);
      result_data = under_image->save(result_fmt, config);
   } 
   catch (const std::exception &e) 
   {
      LOG_ERROR(boost::format("Could not save image: %1%") % e.what());
      return false;
   }

   return true;
}

// a handle with some composited data.
class composite_handle 
   : public rendermq::tile_storage::handle
{
public:
   composite_handle(std::time_t last_mod, bool expired, const string &data) 
      : m_last_modified(last_mod), m_expired(expired), m_data(data)
   {
   }

   ~composite_handle() {}

   bool exists() const { return true; }
   std::time_t last_modified() const { return m_last_modified; }
   bool data(string &data) const 
   {
      data = m_data;
      return true;
   }
   bool expired() const { return m_expired; }

private:
   std::time_t m_last_modified;
   bool m_expired;
   string m_data;
};

bt::ptree get_subtree(const bt::ptree &pt, const string &name)
{
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

   return sub_pt;
}

boost::shared_ptr<rendermq::tile_storage> 
make_subtree(const bt::ptree &pt, boost::optional<zmq::context_t &> ctx, const string &name)
{
   using rendermq::tile_storage;

   bt::ptree sub_pt = get_subtree(pt, name);
   
   // attempt to create the storage
   tile_storage *ptr = rendermq::get_tile_storage(sub_pt, ctx);
   
   if (ptr == NULL)
   {
      throw std::runtime_error((boost::format("Failed to create `%1%' item within composting storage.") % name).str());
   }
   else
   {
      return shared_ptr<tile_storage>(ptr);
   }
}

rendermq::tile_storage *create_compositing_storage(const bt::ptree &pt,
                                                   boost::optional<zmq::context_t &> ctx)
{
   // NOTE: the way we do paths with dots in them is going to cause
   // a bunch of trouble if we ever decide we want to start nesting/
   using rendermq::tile_storage;

   shared_ptr<tile_storage> under = make_subtree(pt, ctx, "under");
   shared_ptr<tile_storage> over  = make_subtree(pt, ctx, "over");
   bt::ptree config = get_subtree(pt, "config");

   return new rendermq::compositing_storage(under, over, config);
}

const bool registered = register_tile_storage("compositing", create_compositing_storage);

} // anonymous namespace

namespace rendermq 
{

compositing_storage::compositing_storage(boost::shared_ptr<tile_storage> under,
                                         boost::shared_ptr<tile_storage> over,
                                         const bt::ptree &config) 
   : m_under_storage(under), m_over_storage(over), m_config(config), 
     m_under_style(m_config.get_optional<string>("under_style")),
     m_over_style(m_config.get_optional<string>("over_style")),
     m_generate_format(fmtNone)
{
   m_under_format = get_format_for(m_config.get<string>("under_format"));
   m_over_format  = get_format_for(m_config.get<string>("over_format"));
   
   // some sanity checking...
   if (m_under_format == fmtNone) 
   {
      throw std::runtime_error((boost::format("Value for composite storage under_format, `%1%', doesn't match a known format type.") % m_config.get<string>("under_format")).str());
   }
   if (m_over_format == fmtNone) 
   {
      throw std::runtime_error((boost::format("Value for composite storage over_format, `%1%', doesn't match a known format type.") % m_config.get<string>("over_format")).str());
   }

   if (m_config.get_child_optional("jpeg")) 
   { 
      m_generate_format = protoFmt(m_generate_format | fmtJPEG); 
   }
   if (m_config.get_child_optional("gif"))
   { 
      m_generate_format = protoFmt(m_generate_format | fmtGIF); 
   }
   if (m_config.get_child_optional("png"))
   { 
      m_generate_format = protoFmt(m_generate_format | fmtPNG); 
   }

   if (m_generate_format == fmtNone) 
   {
      throw std::runtime_error("No generation formats found in composite storage config. Have you set up the format configuration?");
   }   
}

compositing_storage::~compositing_storage() 
{
}

shared_ptr<tile_storage::handle> 
compositing_storage::get(const tile_protocol &tile) const 
{
   // check that we can generate the output format that
   // the requestor wants.
   if (!can_generate_formats(tile.format))
   {
      // no use in trying to get the tile if there's no
      // way to generate the composite...
      LOG_FINER(boost::format("Cannot generate format for tile %1% "
                              "when configured formats are %2%.")
                % tile % m_generate_format);
      return shared_ptr<tile_storage::handle>(new null_handle());
   }

   // modify the requests to set the format type that is 
   // configured - this may well be different from the
   // input type, as it's almost certainly the case that
   // the under tile is opaque (maybe JPG or PNG) and the
   // over tile has an alpha channel (GIF or PNG).
   tile_protocol under_tile(tile); 
   under_tile.format = m_under_format;
   if (m_under_style) { under_tile.style = m_under_style.get(); }

   tile_protocol over_tile(tile);  
   over_tile.format = m_over_format;
   if (m_over_style) { over_tile.style = m_over_style.get(); }

   // try and get the tiles
   shared_ptr<tile_storage::handle> under_handle = m_under_storage->get(under_tile);
   if (under_handle->exists())
   {
      shared_ptr<tile_storage::handle> over_handle = m_over_storage->get(over_tile);
      if (over_handle->exists())
      {
         // get the maximum last-modified time - this is to be
         // conservative about the time so that updates to either 
         // input may be presented to the client. for example, if
         // one layer is relatively static over some period and 
         // gets updated, then the last-modified will reflect 
         // that and clients will not get 304s. for this to work
         // properly, the timestamps on updated layers must be
         // the time at which they were available for compositing.
         // if the time is back-dated (to when they were generated
         // perhaps) then expiry won't work correctly.
         std::time_t last_mod = std::max(under_handle->last_modified(),
                                         over_handle->last_modified());

         // tile is expired if *either* of the input tiles are 
         // expired. this is also conservative - don't want to be
         // assuming some stuff is fresh when it potentially isn't.
         bool expired = under_handle->expired() || over_handle->expired();

         // extract the data from the tiles
         string under_data, over_data, result_data;
         bool data_ok = (under_handle->data(under_data) && 
                         over_handle->data(over_data));
         if (data_ok) 
         {
            data_ok = composite(under_data, m_under_format,
                                over_data, m_over_format,
                                result_data, tile.format,
                                m_config);
         }

         if (data_ok)
         {
            // return a composited tile.
            return shared_ptr<tile_storage::handle>(new composite_handle(last_mod, expired, result_data));
         }
         else
         {
            // return a null tile 
            LOG_ERROR(boost::format("Unable to composite image for tile %1%.") % tile);
            return shared_ptr<tile_storage::handle>(new null_handle());
         }
      }
      else
      {
         // over tile doesn't exist - we need both to exist, so
         // return the null-ish tile.
         LOG_FINER(boost::format("Over tile %1% does not exist.") % over_tile);
         return over_handle;
      }
   }
   else
   {
      // under tile doesn't exist - return a null(ish) tile
      LOG_FINER(boost::format("Under tile %1% does not exist.") % under_tile);
      return under_handle;
   }
}

bool 
compositing_storage::get_meta(const tile_protocol &tile, std::string &data) const {
   std::string under_data, over_data;

   if (m_under_storage->get_meta(tile, under_data))
   {
      if (m_over_storage->get_meta(tile, over_data)) 
      {
         // TODO: do something
         return false;
      }
   }

   return false;
}

bool 
compositing_storage::put_meta(const tile_protocol &tile, const std::string &buf) const 
{
   // can't support this operation - there's not enough information
   // in an already composited tile to allow it to the split into
   // uncomposited parts.
   return false;
}   

bool 
compositing_storage::expire(const tile_protocol &tile) const 
{
   bool expire_under = m_config.get<bool>("expire_under");
   bool expire_over  = m_config.get<bool>("expire_over");
   bool under_ok = !expire_under, over_ok = !expire_over;

   // try to expire the under layer first, if that's what's
   // configured. keep the return value and...
   if (expire_under)
   {
      under_ok = m_under_storage->expire(tile);
   }
   // only try to expire the over layer if it's configured 
   // and the under layer went ok (or wasn't configured).
   // this is trying to be conservative - don't want to do
   // extra stuff above and beyond what's necessary. could
   // lead to some odd effects...
   if (expire_over && under_ok)
   {
      over_ok = m_over_storage->expire(tile);
   }

   return under_ok && over_ok;
}

bool 
compositing_storage::can_generate_formats(protoFmt formats) const 
{
   // need to check that the bits set in formats are a strict
   // subset of the ones configured as available.
   return (formats & m_generate_format) == m_generate_format;
}

} // namespace rendermq

