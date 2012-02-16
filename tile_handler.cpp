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

#include "spherical_mercator.hpp"
#include "tile_protocol.hpp"
#include "mongrel_request.hpp"
#include "storage/meta_tile.hpp"
#include "http/http_reply.hpp"
#include "dqueue/distributed_queue.hpp"
#include "zstream.hpp"
#include "zstream_pbuf.hpp"
#include "storage_worker.hpp"
#include "tile_handler.hpp"
#include "logging/logger.hpp"

// 0MQ
#include <zmq.hpp>

// boost
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string.hpp>

// stl
#include <iostream>
#include <sstream>
#include <fstream>
#include <ctime>
#include <stdexcept>
#include <map>
#include <list>

using boost::shared_ptr;
using boost::optional;
using std::make_pair;
using std::string;
using std::map;
using std::vector;
using std::list;
using std::runtime_error;
namespace pt = boost::property_tree;

// unless otherwise specified, the maximum zoom for any tile
// layer. this can be overridden on a per-style basis in the
// config file.
#define DEFAULT_MAX_ZOOM (18)

namespace {

inline bool old_tile(rendermq::tile_protocol const& tile, std::time_t delta)
{
   return (tile.z > 10 && delta > 31536000) ? true : false;
}

const string mime_json("application/json;charset=UTF-8");
const string mime_png("image/png");
const string mime_jpg("image/jpeg");
const string mime_gif("image/gif");

const string &mime_type_for(rendermq::protoFmt fmt) {
   switch (fmt) {
   case rendermq::fmtPNG:  return mime_png;
   case rendermq::fmtJPEG: return mime_jpg;
   case rendermq::fmtGIF: return mime_gif;
   case rendermq::fmtJSON: return mime_json;
   default:
      throw runtime_error("Ambiguous format in mime_type_for()");
   }
}

// parse a comma- or space-separated list of formats into a 
// protoFmt bitmask.
rendermq::protoFmt parse_formats(const string &str) 
{
   vector<string> splits;
   int fmts = rendermq::fmtNone;

   boost::split(splits, str, boost::is_any_of(", "), boost::token_compress_on);

   for (vector<string>::iterator itr = splits.begin();
        itr != splits.end(); ++itr) 
   {
      string fmt_name = *itr;
      boost::to_lower(fmt_name);

      if (fmt_name == "png") 
      {
         fmts |= rendermq::fmtPNG;
      }
      else if ((fmt_name == "jpg") || (fmt_name == "jpeg"))
      {
         fmts |= rendermq::fmtJPEG;
      }
      else if (fmt_name == "gif")
      {
         fmts |= rendermq::fmtGIF;
      }
      else if (fmt_name == "json")
      {
         fmts |= rendermq::fmtJSON;
      }
      else
      {
         throw runtime_error((boost::format("Unrecognised style format '%1%'.") % fmt_name).str());
      }
   }

   return (rendermq::protoFmt)fmts;
}

inline bool check_xyz(const rendermq::tile_protocol &tile, int max_zoom)
{
   bool bad_coords = ( tile.z < 0 || tile.z > max_zoom);
   if  (!bad_coords)
   {
      int max_xy = (1 << tile.z) -1;
      bad_coords = (tile.x < 0 || tile.x > max_xy 
                    || tile.y < 0 || tile.y > max_xy);
   }
   return not bad_coords;
}

} // anonymous namespace

namespace po = boost::program_options;

namespace rendermq {

tile_handler::tile_handler(const string &handler_id, 
                           const string &in_ep, 
                           const string &out_ep,
                           std::time_t max_age,
                           size_t queue_threshold_stale,
                           size_t queue_threshold_satisfy,
                           size_t queue_threshold_max,
                           bool stale_render_background,
                           size_t max_io_threads,
                           const string &dqueue_config,
                           const pt::ptree &storage_conf,
                           const style_rules &rules,
                           const map<string, list<string> > &dirty_list)
   : m_context(1), 
     m_socket_req(m_context, ZMQ_PULL), 
     m_socket_rep(m_context, ZMQ_PUB),
     m_str_handler_id(handler_id),
     m_max_age(max_age), 
     m_queue_threshold_stale(queue_threshold_stale),
     m_queue_threshold_satisfy(queue_threshold_satisfy),
     m_queue_threshold_max(queue_threshold_max),
     m_stale_render_background(stale_render_background),
     m_style_rules(rules),
     m_queue_runner(dqueue_config, m_context), 
     m_socket_storage_request(m_context),
     m_socket_storage_results(m_context)
{
   LOG_INFO(boost::format("Init tile handler with ID: %1%") % m_str_handler_id);

   // connect the out socket to mongrel, so we've somewhere
   // for requests to go if we happen to receive some the 
   // instant we start up.
   m_socket_rep.setsockopt(ZMQ_IDENTITY, m_str_handler_id.data(), m_str_handler_id.length());        
   m_socket_rep.connect(out_ep.c_str());

   // setup the queue runner
   m_queue_runner.default_handler(
      dqueue::runner::handler_function_t(
         boost::bind(&tile_handler::reply_with_tile, this, _1)));

   // connect input socket to mongrel server
   m_socket_req.connect(in_ep.c_str());
      
   // push and pull sockets for storage handling.
   m_socket_storage_request.bind("inproc://storage_request_" + m_str_handler_id);
   m_socket_storage_results.bind("inproc://storage_results_" + m_str_handler_id);
   
   // start storage worker thread
   m_ptr_storage_instance.reset(new storage_worker(m_context, storage_conf, m_str_handler_id, max_io_threads, dirty_list));
   m_ptr_storage_thread.reset(new boost::thread(boost::ref(*m_ptr_storage_instance)));
}

void 
tile_handler::operator()() {
   // main pull/pub loop     
   while (true) {
      zmq::pollitem_t items [] = {
         //  Always poll for mongrel frontend activity
         { m_socket_req,  0, ZMQ_POLLIN, 0 }, 
         // always poll for storage component activity
         { m_socket_storage_results.socket(), 0, ZMQ_POLLIN, 0 },
         //  Poll tile
         { NULL, 0, ZMQ_POLLIN, 0 },
         { NULL, 0, ZMQ_POLLIN, 0 },
      };
    
      // for the moment assume there's only one pollitem for the distributed queue
      assert(m_queue_runner.num_pollitems() == 2);
      m_queue_runner.fill_pollitems(&items[2]);
    
      // poll
      try {
         zmq::poll(&items[0], 4, -1);
      } catch (const zmq::error_t &) {
         // ignore and loop...
         continue;
      }
    
      // handle request from mongrel
      if (items[0].revents & ZMQ_POLLIN) {
         // this will either send a request to the storage component, or 
         // return an error to the user. either way, it shouldn't take long.
         handle_request_from_mongrel();
      }
                
      // handle response from the storage component
      else if (items[1].revents & ZMQ_POLLIN) {
         // this either returns a response to the client, which might be an
         // error, or forwards the request on to the broker.
         handle_response_from_storage();
      } 
                
      // handle response from broker
      else {
         m_queue_runner.handle_pollitems(&items[2]);
      }
   }
}

void 
tile_handler::reply_with_tile(const tile_protocol &tile) {
   string send_id = (boost::format("%d") % tile.id).str();         
   std::time_t current_time = std::time(0);

   if (tile.id < 0) {
      LOG_ERROR(boost::format("Cannot reply with tile where ID is set to invalid: %1%") % tile);
      return;
   }

   /* tile is done, has data & is OK */
   if ((tile.status == cmdDone || tile.status == cmdIgnore) &&
       (tile.data().size() > 0)) {
      // always assume that the tile is good for another max_age seconds.
      std::time_t expire_time = current_time + m_max_age;
      // what's the expected mime type returned?
      const string &mime_type = mime_type_for(tile.format);
                
      /* tile modified data is younger than last modified header, 
         or last modified header doesn't exist */
      if ((tile.request_last_modified == 0) || 
          (tile.last_modified < tile.request_last_modified)) {
         send_tile(m_socket_rep, m_date_format, m_str_mongrel_id, send_id, 
                   m_max_age, tile.last_modified, expire_time, tile.data(), 
                   mime_type);
                        
      } else {
         // not modified
         send_304(m_socket_rep, m_str_mongrel_id, send_id,
                  current_time, m_date_format, mime_type);
      }
   } else {
      // something bad happened, return a server error status
      send_500(m_socket_rep, m_str_mongrel_id, send_id);
      // log this out too...
      LOG_ERROR(boost::format("tile received from broker is %1% and has status "
                              "!= done/ignore or zero size.") % tile);
   }
}

void 
tile_handler::handle_request_from_mongrel() {
   int64_t more;
   size_t more_size = sizeof (more);
   zmq::message_t msg;
        
   // receive all message parts
   while (true) {
      msg.rebuild();
      m_socket_req.recv(&msg);
      m_socket_req.getsockopt( ZMQ_RCVMORE, &more, &more_size);
      if (!more) break;
   }               
        
   // process last message
   string txt((char*)msg.data(),msg.size());
   mongrel_request request;                
        
   if (m_request_parse(request, txt)) {
      tile_protocol tile;
      if (m_path_parse(tile, request.path()) && 
          m_style_rules.rewrite_and_check(tile)) {
         // need to store the ID of the client in with the tile request so
         // that when/if the data comes back we know where to tell mongrel
         // to send it to.
         tile.id = boost::lexical_cast<int>(request.id());

         // need to store the id of the mongrel server too? we really 
         // should, in case multiple mongrel servers are being used. but
         // for the moment, just assume it's true.
         if (m_str_mongrel_id.empty()) {
            m_str_mongrel_id = request.uuid();
#ifdef RENDERMQ_DEBUG
         } else {
            assert(m_str_mongrel_id == request.uuid());
#endif
         }

         // send request to storage, see if the tile has already been
         // cached.
         m_socket_storage_request << tile;
                        
      } else {
         send_404(m_socket_rep, request.uuid(), request.id());
      }
   }
}

void 
tile_handler::handle_response_from_storage() {
   tile_protocol tile;
   m_socket_storage_results >> tile;
  
   if (tile.status == cmdStatus) {
      string send_id = (boost::format("%d") % tile.id).str(); 
      // request was for status, so the tile metadata will tell us what
      // the response should be.
      if (tile.last_modified > 0) 
      {
         // tile is present, and has a last-modified time
         std::stringstream txt;
         txt << "Tile last modified: " << std::asctime(std::gmtime(&tile.last_modified));
         send_reply(m_socket_rep, m_str_mongrel_id, send_id, 200, txt.str());
         
      } 
      else if (tile.data().size() > 0) 
      {
         // tile is present, but has been expired.
         send_reply(m_socket_rep, m_str_mongrel_id, send_id, 200, "Tile marked as dirty.");
         
      } 
      else 
      {
         // tile isn't present.
         send_404(m_socket_rep, m_str_mongrel_id, send_id);
      }

   } else if (tile.status == cmdDirty) {
      string send_id = (boost::format("%d") % tile.id).str(); 

      if (m_queue_runner.queue_length() >= m_queue_threshold_max)
      {
         // send a 503 - queue is too long to send anything to.
         send_503(m_socket_rep, m_str_mongrel_id, send_id);
      }
      else
      {
         string txt("Tile submitted for rendering...");
         send_reply(m_socket_rep, m_str_mongrel_id, send_id, 200, txt);

         tile.status = cmdRenderBulk;
         tile.set_data("");
         tile.id = -1;
         send_to_queue(tile);
      }
   } else if (tile.status == cmdNotDone) {
      // tile isn't available - have to render it, if there are resources
      // available to do it.
      if (m_queue_runner.queue_length() >= m_queue_threshold_max) 
      {
         // send 503 (service unavailable) to indicate overload.
         string send_id = (boost::format("%d") % tile.id).str(); 
         send_503(m_socket_rep, m_str_mongrel_id, send_id);

      } 
      else if (m_queue_runner.queue_length() >= m_queue_threshold_satisfy)
      {
         // render the tile in the background and tell the client that
         // it's not ready yet.
         string send_id = (boost::format("%d") % tile.id).str(); 
         send_202(m_socket_rep, m_str_mongrel_id, send_id);

         tile.status = cmdRenderBulk;
         tile.set_data("");
         tile.id = -1;
         send_to_queue(tile);

      } 
      else 
      {
         // render the tile (and have the client wait for the response)
         tile.status = cmdRender;
         send_to_queue(tile);
      } 

   } else {
      // check if tile is fresh
      if ((tile.status == cmdDone) ||
          (m_queue_runner.queue_length() >= m_queue_threshold_stale))
      {
         reply_with_tile(tile);
      }
      else
      {
         // if set up to reply instantly and re-render in the background.
         // this will reduce apparent latency to the client, but means 
         // that up-to-date data isn't always what's being served.
         if (m_stale_render_background)
         {
            reply_with_tile(tile);
            // don't background render when the queue is very long. this
            // prevents queue overload when a very large area has been
            // expired.
            if (m_queue_runner.queue_length() < m_queue_threshold_stale)
            {
               tile.status = cmdRenderBulk;
               tile.set_data("");
               tile.id = -1;
               send_to_queue(tile);
            }
         }
         else 
         {
            // otherwise render the tile and wait for the result.
            tile.status = cmdRender;
            send_to_queue(tile);
         }            
      }
   }       
}

style_rules::style_rules(const pt::ptree &conf)
{
   // see if there are any style rewrite rules
   optional<const pt::ptree &> rewrites = conf.get_child_optional("rewrite");
   if (rewrites)
   {
      for (pt::ptree::const_iterator itr = rewrites->begin();
           itr != rewrites->end(); ++itr) 
      {
         string from_style = itr->first;
         string to_style = rewrites->get<string>(from_style);
         m_rewrites.insert(make_pair(from_style, to_style));
      }
   }

   // see if there are any style format listings
   optional<const pt::ptree &> formats = conf.get_child_optional("formats");
   if (formats)
   {
      for (pt::ptree::const_iterator itr = formats->begin();
           itr != formats->end(); ++itr) 
      {
         string style = itr->first;
         string fmt_str = formats->get<string>(style);
         protoFmt fmts = parse_formats(fmt_str);
         m_formats.insert(make_pair(style, fmts));
      }
   }

   // see if we're forcing the return types. yeah, it's a nasty hack
   // and we should lean on the toolkit guys to get rid of this as
   // soon as we can...
   optional<const pt::ptree &> forced_formats = conf.get_child_optional("forced_formats");
   if (forced_formats)
   {
      for (pt::ptree::const_iterator itr = forced_formats->begin();
           itr != forced_formats->end(); ++itr) 
      {
         string style = itr->first;
         string fmt_str = forced_formats->get<string>(style);
         protoFmt fmt = get_format_for(fmt_str);
         if (fmt == fmtNone)
         {
            throw std::runtime_error((boost::format("In [forced_formats] for style `%1%', the string `%2%' is not recognised as a format.") % style % fmt_str).str());
         }
         m_forced_formats.insert(make_pair(style, fmt));
      }
   }

   // find out what the max zoom levels are for each tile so
   // that the check can check whether or not the tile is
   // within the world, as far as TMS coords are concerned.
   optional<const pt::ptree &> zoom_limits = conf.get_child_optional("zoom_limits");
   if (zoom_limits) 
   {
      for (pt::ptree::const_iterator itr = zoom_limits->begin();
           itr != zoom_limits->end(); ++itr) 
      {
         string style = itr->first;
         int max_zoom = zoom_limits->get<int>(style);
         m_zoom_limits.insert(make_pair(style, max_zoom));
      }
   }
}

bool
style_rules::rewrite_and_check(tile_protocol &tile) const
{
   // rewrite the tile's style, if there's a rule for it.
   map<string, string>::const_iterator rewrite_itr = m_rewrites.find(tile.style);
   if (rewrite_itr != m_rewrites.end())
   {
      tile.style = rewrite_itr->second;
   }

   // now, if present, force the format to what the style will
   // actually return. this should only be used for compatibiltity
   // with the bug in the toolkit which makes it ask for pngs and
   // expect (style-dependent) a jpg or gif response.
   map<string, protoFmt>::const_iterator forced_itr = m_forced_formats.find(tile.style);
   if (forced_itr != m_forced_formats.end())
   {
      tile.format = forced_itr->second;
   }

   // check the coords for this particular tile, and if the check
   // fails then bomb out immediately.
   map<string, int>::const_iterator zoom_itr = m_zoom_limits.find(tile.style);
   int max_zoom = (zoom_itr != m_zoom_limits.end()) ? zoom_itr->second : DEFAULT_MAX_ZOOM;
   if (!check_xyz(tile, max_zoom)) 
   {
      return false;
   }

   // check the formats... if there are no formats then assume
   // that all formats and styles are permissable, since the 
   // formats section is supposed to be *optional*
   map<string, protoFmt>::const_iterator fmt_itr = m_formats.find(tile.style);
   if (fmt_itr == m_formats.end()) 
   {
      // shouldn't deny when the formats list is empty, as it
      // means the section wasn't specified.
      return m_formats.empty();
   }
   else
   {
      // mapped value is a bitmask of available formats
      return (fmt_itr->second & tile.format) > 0;
   }
}

void
tile_handler::send_to_queue(const rendermq::tile_protocol &tile)
{
   bool error = true;

   try
   {
      m_queue_runner.put_job(tile);
      error = false;
   }
   catch (const dqueue::broker_error &e)
   {
      LOG_ERROR(boost::format("Queue error sending job %1% to queue: %2%.") % tile % e.what());
   }
   catch (const std::exception &e)
   {
      LOG_ERROR(boost::format("Runtime error sending job %1% to queue: %2%.") % tile % e.what());
   }
   catch ( ... )
   {
      LOG_ERROR(boost::format("Unknown error sending job %1% to queue.") % tile);
   }

   // if there was an error, and the tile has a client ID attached to
   // it, then send an error back to the client.
   if (error && tile.id > 0)
   {
      string send_id = (boost::format("%d") % tile.id).str(); 
      send_404(m_socket_rep, m_str_mongrel_id, send_id);
   }
}

} // namespace rendermq


