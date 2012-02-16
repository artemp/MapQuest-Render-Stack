/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
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

#ifndef TILE_HANDLER_HPP
#define TILE_HANDLER_HPP

#include "tile_protocol.hpp"
#include "zstream.hpp"
#include "storage_worker.hpp"
#include "dqueue/distributed_queue.hpp"
#include "tile_path_parser.hpp"
#include "mongrel_request_parser.hpp"
#include "http/http_date_formatter.hpp"

// boost
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>

// stl
#include <ctime>

namespace rendermq {

/* utility object to handle style re-writing and available format
 * filtering.
 *
 * strictly, this isn't necessary, as the worker will perform this
 * check itself. however, it's better to fail early, and not put the
 * job into the queue in the first place.
 *
 * this could be done by reading the worker's configuration, however
 * it's a little nasty to read some other process' config file and it
 * increases unnecessary coupling and brittleness. also, at the 
 * handler level we get to implement proper transparent re-writing,
 * so that the cache is shared between styles.
 */
class style_rules 
{
public:
   // read sections from the given config file.
   explicit style_rules(const boost::property_tree::ptree &);

   // rewrite the style of the tile according to the style rewrite
   // rules and return whether the result matches the available
   // formats.
   bool rewrite_and_check(tile_protocol &tile) const;

private:
   // map of from-style to to-style names.
   std::map<std::string, std::string> m_rewrites;

   // maps the name (after rewriting) into the available formats.
   std::map<std::string, protoFmt> m_formats;

   // maps the style name to the format that it is forced to, in
   // order to maintain compatibility with the way the toolkit
   // works at the moment.
   std::map<std::string, protoFmt> m_forced_formats;

   // zoom level limits, per style.
   std::map<std::string, int> m_zoom_limits;
};

/* handler main loop object.
 *
 * mongrel2 only provides HTTP protocol support, it delegates most of
 * the work of figuring out what the requests mean to 'handler'
 * processes, which run outside of the mongrel process, possibly even
 * on another computer.
 *
 * this handler deals with requests for tiles. it communicates with a
 * storage object to figure out whether that tile exists already and
 * with a distributed rendering queue to render it if not.
 *
 * the handler routes messages between three processes; mongrel2, the
 * storage object and the distributed rendering queue. these other
 * processes may do work, but the handler effectively does none other
 * than routing the messages. this is by design, so that the handler
 * is able to spend as much time as possible in its event loop,
 * reducing the latency for messages to be appropriately routed.
 */
class tile_handler {
public:
   /* construct tile handler, but don't start running yet. this sets
    * up the sockets and subordinate threads, but processes no
    * requests.
    *
    * @param handler_id zeromq identity of this handler.
    * @param in_ep incoming endpoint from mongrel2.
    * @param out_ep outgoing endpoing to mongrel2.
    * @param max_age age, in seconds, to put in the HTTP expiry
    *          headers.
    * @param queue_threshold_stale queue length at which to return
    *          stale tiles rather than render them. 
    * @param queue_threshold_max queue length at which to return an
    *          error rather than try to render tiles. 
    * @param stale_render_background if true, return stale tiles
    *          immediately, even if the queue length is low, and
    *          render the tile in the background.
    * @param max_io_threads maximum number of concurrent storage
    *          requests to run. others are queued.
    * @param dqueue_config file name of distributed queue config.
    * @param storage_conf storage configuration - already parsed as a
    *          property tree.
    * @param rules the style acceptance / re-write rules.
    * @param dirty_list a map of styles into a list of dependent
    *          styles to expire in addition to any specified in a 
    *          dirty request.
    */
   tile_handler(const std::string &handler_id, 
                const std::string &in_ep, 
                const std::string &out_ep,
                std::time_t max_age,
                size_t queue_threshold_stale, 
                size_t queue_threshold_satisfy, 
                size_t queue_threshold_max,
                bool stale_render_background,
                size_t max_io_threads,
                const std::string &dqueue_config,
                const boost::property_tree::ptree &storage_conf,
                const style_rules &rules,
                const std::map<std::string, std::list<std::string> > &dirty_list);
   
   /* run the event loop for the handler.
    */
   void operator()();

private:
   /* return the tile (or an error) to mongrel, depending on the
    * status of the tile. this is also called when a message from the
    * rendering queue is detected.
    */
   void reply_with_tile(const rendermq::tile_protocol &tile);
   
   /* called when a message from mongrel is detected. reads and parses
    * the request message and routes it appropriately.
    */
   void handle_request_from_mongrel();
   
   /* called when a message from the storage object is detected.
    */
   void handle_response_from_storage();

   /* send a tile to the queue. if there's an error then print a 
    * message and, if there is a connection id associated with the
    * tile, send an error response back to the client.
    */
   void send_to_queue(const rendermq::tile_protocol &tile);
   
   // zeromq socket context used in the handler
   zmq::context_t m_context;

   // sockets connected to mongrel. req to receive requests, rep to
   // send replies.
   zmq::socket_t m_socket_req, m_socket_rep;

   // handler identity, used to identify this handler to the broker.
   // must be unique across all handlers for the broker to route
   // messages here.
   const std::string m_str_handler_id;

   // the maximum age in seconds for the handler to send back in the
   // cache-related HTTP headers.
   std::time_t m_max_age;

   // the queue length at which to return stale tiles and errors to the
   // client, respectively.
   size_t m_queue_threshold_stale, m_queue_threshold_satisfy, m_queue_threshold_max;

   // if true, return tiles to the client even if they're stale (marked as
   // expired). this reduces the amount of time clients are waiting for
   // tiles, but means they won't see the latest tiles as soon as possible.
   bool m_stale_render_background;

   // the style re-write rules.
   const style_rules &m_style_rules;

   // the queue of rendering jobs
   dqueue::runner m_queue_runner;
   
   // sockets for sending requests and receiving results from the storage
   // worker thread, respectively.
   zstream::socket::push m_socket_storage_request;
   zstream::socket::pull m_socket_storage_results;

   // mongrel2 format request parser
   rendermq::request_parser m_request_parse;

   // function object to parse URLs into tile protocol objects
   rendermq::tile_path_parser m_path_parse;

   // http time formatting function object
   rendermq::http_date_formatter m_date_format;

   // mongrel2 server ID that we're connected to.
   std::string m_str_mongrel_id;

   // pointers to the instance of the storage worker and the thread that
   // it is running on. this is separate from the main thread of the tile
   // handler so that it can run blocking file / HTTP operations without
   // affecting the main thread's ability to continue handling tiles.
   boost::shared_ptr<rendermq::storage_worker> m_ptr_storage_instance;
   boost::shared_ptr<boost::thread> m_ptr_storage_thread;
};

} // namespace rendermq

#endif /* TILE_HANDLER_HPP */
