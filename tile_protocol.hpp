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


#ifndef TILE_PROTOCOL_HPP
#define TILE_PROTOCOL_HPP

#include <ostream>
#include <zmq.hpp>
#include <boost/unordered_map.hpp>
#include <cstring> // for memcpy
#include <ctime> // for std::time_t
#include "tile_utils.hpp"
#include "tile_protocol.hpp"
#include "proto/tile.pb.h"
#include "storage/meta_tile.hpp" // for METATILE size
#include <iostream>
#include <vector>

namespace rendermq
{

enum protoCmd { 
   cmdIgnore,    
   cmdRender,     // render with normal priority
   cmdDirty,      // expire a tile (and submit for re-rendering?)
   cmdDone,       // worker says command completed successfully
   cmdNotDone, 
   cmdRenderPrio, // render with higher priority
   cmdRenderBulk, // render with lower priority, and don't expect a response.
   cmdStatus      // request the status of a tile
};

class tile_protocol
{

public:
   tile_protocol()
      : status(cmdRenderPrio), x(0), y(0), z(0), id(0), style(""), format(fmtPNG), last_modified(0), request_last_modified(0) {}
   tile_protocol(protoCmd status_,int x_,int y_, int z_, int64_t id_, const std::string & style_, protoFmt format_, std::time_t last_mod_=0, std::time_t req_last_mod_=0)
      : status(status_), x(x_), y(y_), z(z_), id(id_), style(style_), format(format_), last_modified(last_mod_), request_last_modified(req_last_mod_) {}
   tile_protocol(tile_protocol const& other)
      : status(other.status), 
        x(other.x), y(other.y), 
        z(other.z), id(other.id),
        style(other.style),
        format(other.format),
        last_modified(other.last_modified),
        request_last_modified(other.request_last_modified),
        data_(other.data_)
      {}
    
   std::string const& data() const
      {
         return data_;
      }
   void set_data(std::string const& data)
      {
         data_ = data;
      }

   protoCmd status;
   int x;
   int y;
   int z;
   int64_t id;
   std::string style;
   protoFmt format;
   std::time_t last_modified;
   std::time_t request_last_modified;

private:
   std::string data_;
};

inline std::ostream& operator<< (std::ostream& out, tile_protocol const& t)
{
   out << "TILE PROTO " << t.z << ":" << t.x << ":" << t.y << " status=";

   if (t.status == cmdIgnore) { out << "cmdIgnore"; }
   else if (t.status == cmdRender) { out << "cmdRender"; }
   else if (t.status == cmdDirty) { out << "cmdDirty"; }
   else if (t.status == cmdDone) { out << "cmdDone"; }
   else if (t.status == cmdNotDone) { out << "cmdNotDone"; }
   else if (t.status == cmdRenderPrio) { out << "cmdRenderPrio"; }
   else if (t.status == cmdRenderBulk) { out << "cmdRenderBulk"; }
   else if (t.status == cmdStatus) { out << "cmdStatus"; }
   else { out << "[[unrecognised_command]]"; }

   { // output the format in a nice, human-readable way.
      out << " fmt=";
      std::vector<protoFmt> protoFmts = get_formats_vec(t.format);
      for(std::vector<protoFmt>::const_iterator protoFmt = protoFmts.begin(); protoFmt != protoFmts.end(); protoFmt++)
      {
         if(protoFmt != protoFmts.begin())
            out << ",";
         out << file_type_for(*protoFmt);
      }
   }

   if (t.last_modified > 0) { out << " last_modified=" << t.last_modified; }
   if (t.request_last_modified > 0) { out << " request_last_modified=" << t.request_last_modified; }

   out << " id=" << t.id << " style=" << t.style
       << " data.size()=" << t.data().size() ;
   return out;
}

inline bool operator==(tile_protocol const &a, tile_protocol const &b) {
   // note: we're missing out the status, since that's expected to change.
   return 
      a.x == b.x && a.y == b.y && a.z == b.z && 
      a.id == b.id && a.style == b.style &&
      a.format == b.format;
}

inline bool operator!=(tile_protocol const &a, tile_protocol const &b) {
   return !operator==(a, b);
}

inline size_t hash_value(const tile_protocol &tp) {
   size_t seed = 0;
   // note: client id is deliberately excluded from this because we want
   // tiles for multiple clients to be hashed to the same value for 
   // distribution to and collapsing within the brokers. the same goes
   // for the format - the worker will do all the formats which are
   // available on that style anyway (e.g: no JPEG for transparent styles,
   // no JSON for non-clickable styles).
   boost::hash_combine(seed, tp.style);
   boost::hash_combine(seed, tp.z);
   boost::hash_combine(seed, tp.x & ~(METATILE - 1));
   boost::hash_combine(seed, tp.y & ~(METATILE - 1));
   return seed;
}

inline bool serialise(const tile_protocol &tile, std::string &buf) {
   proto::tile t;
   t.set_command(tile.status);
   t.set_x(tile.x);
   t.set_y(tile.y);
   t.set_z(tile.z);
   t.set_id(tile.id);
   t.set_image(tile.data());
   t.set_style(tile.style);
   t.set_format(tile.format);
   if (tile.last_modified != 0) { t.set_last_modified(tile.last_modified); }
   if (tile.request_last_modified != 0) { t.set_request_last_modified(tile.request_last_modified); }
   return t.SerializeToString(&buf);
}

inline bool unserialise(const std::string &buf, tile_protocol &tile) {
   proto::tile t;
   bool result = t.ParseFromString(buf);
   if (result) {
      tile.status = static_cast<rendermq::protoCmd>(t.command());
      tile.x = t.x();
      tile.y = t.y();
      tile.z = t.z();
      tile.id = t.id();
      tile.set_data(t.image());
      tile.style = t.style();
      tile.format = static_cast<rendermq::protoFmt>(t.format());
      tile.last_modified = t.has_last_modified() ? t.last_modified() : 0;
      tile.request_last_modified = t.has_request_last_modified() ? t.request_last_modified() : 0;
   }
   return result;
}

inline bool send(zmq::socket_t & socket, tile_protocol const& tile)
{
   std::string buf;
   if (serialise(tile, buf))
   {
      zmq::message_t msg(buf.size()); 
      std::memcpy(msg.data(),buf.data(),buf.size());
      return socket.send(msg);
   }
   return false;
}

inline bool send_to(const std::string id, zmq::socket_t & socket, tile_protocol const& tile) {
   std::string buf;
   if (serialise(tile, buf)) {
      zmq::message_t msg(id.size()); 
      std::memcpy(msg.data(),id.data(),id.size());

      // send the ID of the receiver first
      if (!socket.send(msg, ZMQ_SNDMORE)) return false;
      msg.rebuild();

      // then a blank spacer message
      if (!socket.send(msg, ZMQ_SNDMORE)) return false;
      msg.rebuild(buf.size());
    
      // then send the message itself
      std::memcpy(msg.data(),buf.data(),buf.size());
      return socket.send(msg);
   }
   return false;  
}

inline bool recv (zmq::socket_t & socket, tile_protocol & tile)
{
   zmq::message_t msg;
   socket.recv(&msg);
   std::string buf(static_cast<char*>(msg.data()),msg.size());
   return unserialise(buf, tile);
}

}

#endif // TILE_PROTOCOL_HPP
