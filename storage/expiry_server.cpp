/*------------------------------------------------------------------------------
 *
 * Implementation of the expiry service, a redundant (2x) server
 * to handle queries as to whether a tile is expired or not.
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

#include "expiry_server.hpp"
#include "../zstream_pbuf.hpp"
#include "meta_tile.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <google/sparse_hash_set>
#include <list>
#include <map>

#define HEARTBEAT (1000000)

// simple define for logging, leaves less clutter...
#ifdef RENDERMQ_DEBUG
#define LOG_MSG(x) std::cout << (x) << std::endl
#else
#define LOG_MSG(x)
#endif

using std::string;
using std::list;
using std::map;
using std::pair;
using std::make_pair;
using boost::shared_ptr;
namespace pt = boost::property_tree;
namespace dt = boost::posix_time;
namespace manip = zstream::manip;

namespace 
{
uint32_t tile_to_frag(const rendermq::tile_protocol &tile) 
{
   // we assume that METATILE is 8 here, and use that to pack
   // tile information into a 32-bit struct. we further assume
   // that the max zoom level is 18.
#if METATILE != 8
#error "This code is configured and optimised on the assumption that METATILE=8."
#endif

   uint32_t xy_frag = (uint32_t(tile.x >> 3) << (tile.z - 3)) | uint32_t(tile.y >> 3);
   if (tile.z == 18) 
   {
      return xy_frag;
   }
   else if (tile.z > 13)
   {
      return (uint32_t(4 | (17 - tile.z)) << 28) | xy_frag;
   }
   else
   {
      return (uint32_t(32 | (13 - tile.z)) << 26) | xy_frag;
   }
}

void frag_to_tile(uint32_t frag, rendermq::tile_protocol &tile) 
{
   uint32_t high_bits = frag >> 30;

   if (high_bits == 0) 
   {
      tile.z = 18;
   }
   else if (high_bits == 1)
   {
      tile.z = 17 - ((frag >> 28) & 3);
   }
   else 
   {
      tile.z = 13 - ((frag >> 16) & 15);
   }

   uint32_t mask = (1 << tile.z) - 1;
   tile.x = (frag >> tile.z) & mask;
   tile.y = frag & mask;
}
}

namespace rendermq
{

struct expiry_server::fsm 
{
   fsm(state_t init) : m_state(init) {}

   // returns false if the event was bad - i.e: a disallowed
   // state transform, or leaves the fsm in a bad state.
   bool event(event_t ev) {
      bool status_ok = true;

      if (m_state == state_primary)
      {
         if (ev == event_peer_backup)
         {
            LOG_MSG("primary connected to backup (slave), ready as master");
            m_state = state_active;
         }
         else if (ev == event_peer_active)
         {
            LOG_MSG("primary connected to backup (master), ready as slave");
            m_state = state_passive;
         }
      }
      else if (m_state == state_backup)
      {
         if (ev == event_peer_active)
         {
            LOG_MSG("backup connected to primary (master), ready as slave");
            m_state = state_passive;
         }
         else if (ev == event_client_request)
         {
            LOG_MSG("backup got unexpected client request");
            status_ok = false;
         }
      }
      else if (m_state == state_active)
      {
         if (ev == event_peer_active)
         {
            std::cerr << "ERROR: Fatal error - both primary and backup servers think they're masters." << std::endl;
            status_ok = false;
         }
      }
      else if (m_state == state_passive)
      {
         if (ev == event_peer_primary)
         {
            LOG_MSG("backup: primary (slave) is restarting, ready as master.");
            m_state = state_active;
         }
         else if (ev == event_peer_backup)
         {
            LOG_MSG("primary: backup (slave) is restarting, ready as master.");
            m_state = state_active;
         }
         else if (ev == event_peer_passive)
         {
            std::cerr << "ERROR: Fatal error - both primary and backup servers think they're slaves." << std::endl;
            status_ok = false;
         }
         else if (ev == event_client_request)
         {
            if (dt::microsec_clock::universal_time() >= m_peer_expiry)
            {
               LOG_MSG("failover! ready as master.");
               m_state = state_active;
            }
            else 
            {
               status_ok = false;
            }
         }
      }
      
      if (status_ok && (ev != event_client_request)) 
      {
         m_peer_expiry = dt::microsec_clock::universal_time() + dt::microseconds(2 * HEARTBEAT);
      }

      return status_ok;
   }

   state_t m_state;
   dt::ptime m_peer_expiry;
};

struct style_and_format
{
   style_and_format() : style(""), format(0) {}
   explicit style_and_format(const tile_protocol &tile)
      : style(tile.style), format(tile.format) {}

   inline bool operator<(const style_and_format &s) const 
   {
      return (style < s.style) ||
         ((style == s.style) && (format < s.format));
   }
   inline bool operator==(const style_and_format &s) const
   {
      return (style == s.style) && (format == s.format);
   }

   string style;
   int format;
};

struct expiry_server::expiry_data 
{
   typedef google::sparse_hash_set<uint32_t> hash_set_t;
   typedef map<style_and_format, shared_ptr<hash_set_t> > style_to_hash_t;

   expiry_data() {}
   ~expiry_data() {}

   bool lookup(const tile_protocol &t) const 
   {
      style_to_hash_t::const_iterator itr = m_hash_sets.find(style_and_format(t));
      if (itr != m_hash_sets.end())
      {
         hash_set_t::const_iterator jtr = itr->second->find(tile_to_frag(t));
         return jtr != itr->second->end();
      }
      return false;
   }

   bool insert(const tile_protocol &t) 
   {
      style_and_format sf(t);
      style_to_hash_t::iterator itr = m_hash_sets.find(sf);

      // if the sparse hash we want to insert into isn't there
      // then create it.
      if (itr == m_hash_sets.end())
      {
         shared_ptr<hash_set_t> hash_ptr(new hash_set_t());
         hash_ptr->set_deleted_key(0xffffffffUL);

         pair<style_to_hash_t::iterator, bool> ptr = 
            m_hash_sets.insert(make_pair(sf, hash_ptr));
         assert(ptr.second);
         
         ptr.first->second->insert(tile_to_frag(t));
      }
      else 
      {
         itr->second->insert(tile_to_frag(t));
      }

      return true;
   }

   bool erase(const tile_protocol &t)
   {
      style_and_format sf(t);
      style_to_hash_t::iterator itr = m_hash_sets.find(sf);

      if (itr != m_hash_sets.end())
      {
         hash_set_t &set = *(itr->second);
         hash_set_t::const_iterator jtr = set.find(tile_to_frag(t));
         if (jtr != set.end())
         {
            set.erase(jtr);
         }
      }
      return true;
   }

   style_to_hash_t m_hash_sets;
};

expiry_server::expiry_server(zmq::context_t &ctx,
                             state_t init_state,
                             const pt::ptree &conf)
   : m_context(ctx),
     m_socket_frontend(ctx),
     m_socket_statepub(ctx),
     m_socket_statesub(ctx),
     m_fsm(new fsm(init_state)),
     m_expired(new expiry_data())
{
   if (init_state == state_primary) 
   {
      m_socket_frontend.bind(conf.get<string>("primary.frontend"));
      m_socket_statepub.bind(conf.get<string>("primary.statepub"));
      m_socket_statesub.connect(conf.get<string>("backup.statepub"));
   }
   else
   {
      m_socket_frontend.bind(conf.get<string>("backup.frontend"));
      m_socket_statepub.bind(conf.get<string>("backup.statepub"));
      m_socket_statesub.connect(conf.get<string>("primary.statepub"));
   }
}

expiry_server::~expiry_server() 
{
}

void 
expiry_server::operator()()
{
   dt::ptime next_heartbeat = dt::microsec_clock::universal_time() + dt::microseconds(HEARTBEAT);
   
   while (true) 
   {
      zmq::pollitem_t items[] = {
         { m_socket_frontend.socket(), 0, ZMQ_POLLIN, 0 },
         { m_socket_statesub.socket(), 0, ZMQ_POLLIN, 0 }
      };

      dt::time_duration wait_time = next_heartbeat - dt::microsec_clock::universal_time();

      try
      {
         zmq::poll(items, 2, wait_time.total_microseconds());
      }
      catch (const std::exception &e)
      {
         std::cerr << "ERROR: " << e.what() << ". Shutting down.\n";
         break;
      }

      if (items[0].revents & ZMQ_POLLIN)
      {
         if (m_fsm->event(event_client_request)) 
         {
            // handle client request
            list<string> addresses;
            tile_protocol tile;
            bool response = false;

            manip::routing_headers headers(addresses);
            m_socket_frontend >> headers
                              >> tile;
            
            if (m_socket_frontend.has_more())
            {
               // command to set the status of that tile
               uint32_t value = 0;
               m_socket_frontend >> value;

               if (value != 0) 
               {
                  response = m_expired->insert(tile);
               }
               else
               {
                  response = m_expired->erase(tile);
               }
            }
            else
            {
               // was a query for the state of that tile
               response = m_expired->lookup(tile);
            }

            m_socket_frontend.to(addresses) << uint32_t(response ? 1 : 0);
         }
         else
         {
            break;
         }
      }
      
      if (items[1].revents & ZMQ_POLLIN)
      {
         uint32_t ev;
         m_socket_statesub >> ev;
         if (!m_fsm->event(event_t(ev)))
         {
            break;
         }
      }

      if (dt::microsec_clock::universal_time() >= next_heartbeat) 
      {
         m_socket_statepub << uint32_t(m_fsm->m_state);
         next_heartbeat += dt::microseconds(HEARTBEAT);
      }
   }
}

}

