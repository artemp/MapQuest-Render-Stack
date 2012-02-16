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

#ifndef STORAGE_WORKER_HPP
#define STORAGE_WORKER_HPP

#include "zstream.hpp"
#include "tile_protocol.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <string>
#include <list>

namespace rendermq {

/* threaded storage worker. accepts requests for tiles to be looked up
 * in the storage on an inproc set of sockets and spawns threads to
 * handle the blocking storage requests.
 *
 * it would have been better to use non-blocking I/O or AIO for this,
 * but that's not something that's supported by NFS.
 *
 * TODO: add queue so that large number of requests doesn't fork-bomb
 * the machine the handler is on.
 */
class storage_worker {
public:
   /* constructs a storage worker which listens on 0MQ sockets 
    * inproc://storage_request_${handler_id} and puts results onto
    * inproc://storage_results_${handler_id}. 
    *
    * @param ctx the 0MQ context to use for creating sockets.
    * @param c the config for creating storage drivers.
    * @param handler_id a string used to namespace the inproc
    *    sockets.
    * @param max_concur the maximum number of threads to create to
    *    process storage requests.
    * @param dirty_list a map of styles into a list of dependent
    *    styles to expire in addition to any specified in a dirty
    *    request.
    */
   storage_worker(zmq::context_t &ctx, 
                  const boost::property_tree::ptree &c,
                  const std::string &handler_id,
                  size_t max_concur,
                  const std::map<std::string, std::list<std::string> > &dirty_list); 

   ~storage_worker();
  
   // main loop
   void operator()();
  
private:
   static void thread_func(const boost::property_tree::ptree &conf, 
                           zmq::context_t &ctx,
                           const std::map<std::string, std::list<std::string> > &dirty_list,
                           volatile bool &shutdown_requested,
                           std::string resp_ep, std::string reqs_ep);
  
   // context for zeromq operations
   zmq::context_t &m_context;

   // streams for requests and responses
   zstream::socket::pull requests_in;
   zstream::socket::push results_out;

   // stream for responses from sub-threads
   zstream::socket::pull threads_in;
   zstream::socket::push threads_out;
  
   // maximum number of i/o threads to run. everything else gets queued.
   size_t max_concurrency;

   // the current number of i/o threads busy with work.
   size_t cur_concurrency;
  
   // storage configuration
   const boost::property_tree::ptree &conf;

   // map of style names into a list of style names which are dependent
   // and should be dirtied whenever the keyed style is dirtied.
   std::map<std::string, std::list<std::string> > m_dirty_list;

   // signal to threads when they must shut down
   volatile bool m_shutdown_requested;
  
   // threads which are currently running i/o actions.
   typedef std::list<boost::shared_ptr<boost::thread> > thread_list_t;
   thread_list_t threads;

   // queue of requests which didn't get processed because of the limit 
   // on i/o threads.
   std::list<boost::shared_ptr<tile_protocol> > queued_requests;
};

} // namespace rendermq

#endif /* STORAGE_WORKER_HPP */
