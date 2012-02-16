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

#include "storage_worker.hpp"
#include "storage/tile_storage.hpp"
#include "zstream_pbuf.hpp"
#include "logging/logger.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

#include <signal.h> // to ignore child termination signals

// poll loop timeout in microseconds. this is currently set
// to one second, as it seems better not to have it block
// forever for something that might not come... 
#define STORAGE_WORKER_POLL_TIMEOUT (1000000)

// how often to check for worker thread death, in milli-
// seconds. this should be larger than the poll timeout 
// above.
#define CHECK_THREAD_DEATH_INTERVAL (5*STORAGE_WORKER_POLL_TIMEOUT)

namespace pt = boost::property_tree;
namespace bt = boost::posix_time;
using boost::shared_ptr;
using boost::make_shared;
using std::pair;
using std::make_pair;
using std::map;
using std::list;
using std::string;

namespace rendermq {

namespace {
void handle_tile(tile_protocol &tile,
                 shared_ptr<tile_storage> storage,
                 const map<string, list<string> > &dirty_list) 
{
   if (tile.status == cmdDirty) 
   {
      // the dirty 'status', or command tells us that the user has
      // said that the tile needs to be re-rendered, so first we must
      // expire it from the storage.
      storage->expire(tile);
      
      // check to see what other styles need to be dirtied dependent 
      // on this one.
      map<string, list<string> >::const_iterator itr = dirty_list.find(tile.style);
      if (itr != dirty_list.end()) 
      {
         BOOST_FOREACH(string style, itr->second) 
         {
            tile_protocol dependent_tile(tile);
            dependent_tile.style = style;
            storage->expire(dependent_tile);
         }
      }
   }   
   else // command is not to dirty the tile
   {
      boost::shared_ptr<tile_storage::handle> handle = storage->get(tile);
      
      if (handle->exists()) 
      {
         // don't change the status if the command was for the status,
         // otherwise the handler won't know it was a status query and
         // not a render request.
         if (tile.status != cmdStatus)
         {
            if (handle->expired()) 
            {
               tile.status = cmdIgnore;
            } 
            else 
            {
               tile.status = cmdDone;
            }
         }
         
         std::string data;
         handle->data(data);
         tile.set_data(data);
         tile.last_modified = handle->last_modified();
      }
      else 
      {
         // this case is indicated to the status command by the lack
         // of data... yeah, it's a bit messy... the whole status /
         // command thing could do with a refactor.
         if (tile.status != cmdStatus) 
         {
            tile.status = cmdNotDone;
         }
      }
   }
}
} // anonymous namespace

void 
storage_worker::thread_func(const pt::ptree &conf, 
                            zmq::context_t &ctx,
                            const map<string, list<string> > &dirty_list,
                            volatile bool &shutdown_requested,
                            string resp_ep, string reqs_ep) 
{
   boost::shared_ptr<tile_storage> storage(get_tile_storage(conf, ctx));
   // check that storage initialised correctly
   if (!storage) {
      LOG_ERROR("Couldn't instantiate storage, sending direct to queue.");
      return;
   }

   // create socket and connect anonymously to the storage worker
   zstream::socket::push socket_out(ctx);
   zstream::socket::pull socket_in(ctx);

   socket_out.connect(resp_ep);
   socket_in.connect(reqs_ep);
   
   // local tile object
   tile_protocol tile;

   // event loop
   while (true) 
   {
      if (shutdown_requested)
      {
         break;
      }

      zmq::pollitem_t items [] = {
         { socket_in.socket(), 0, ZMQ_POLLIN, 0 }
      };
      
      try 
      {
         zmq::poll(items, 1, STORAGE_WORKER_POLL_TIMEOUT);
      }
      catch (const zmq::error_t &) 
      {
         // error can be thrown in here due to interrupted system calls. this
         // can be because ctrl-C was pressed, or the process is being run 
         // via a debugger. i haven't yet seen a case where this error isn't
         // ignorable.
         continue;
      }

      if (items[0].revents & ZMQ_POLLIN) {
         try {
            // read the tile request
            socket_in >> tile;

         } 
         catch (const std::exception &e)
         {
            LOG_ERROR("Error during tile receive. Skipping.");
            continue;
         }
            
         try
         {
            // start the stopwatch
            bt::ptime begin = bt::microsec_clock::local_time();
            
            // do the actual work
            handle_tile(tile, storage, dirty_list);
            
            // stop the stopwatch and print warning if the process took
            // too long...
            bt::ptime end = bt::microsec_clock::local_time();
            if ((end - begin) > bt::seconds(5))
            {
               LOG_WARNING(boost::format("Took %1% seconds to fetch %2% from storage.") % (end - begin) % tile);
            }
         }
         catch (const std::exception &e)
         {
            // set tile to "not done" status
            tile.status = cmdNotDone;
            LOG_ERROR(boost::format("Exception during storage activity: %1%, sending "
                                    "'not done' response.") 
                      % e.what());
         }

         // send response back
         socket_out << tile;
      }
   }
}      
  
storage_worker::storage_worker(zmq::context_t &ctx, 
                               const pt::ptree &c,
                               const std::string &handler_id,
                               size_t max_concur,
                               const map<string, list<string> > &dirty_list) 
   : m_context(ctx), requests_in(m_context), results_out(m_context), 
     threads_in(m_context), threads_out(m_context), max_concurrency(max_concur), 
     cur_concurrency(0), conf(c), m_dirty_list(dirty_list),
     m_shutdown_requested(false)
{
   requests_in.connect("inproc://storage_request_" + handler_id);
   results_out.connect("inproc://storage_results_" + handler_id);

   // bind sockets to talk to all the sub-threads which are doing
   // the work of talking to the storage.
   const string thread_in_ep =  "inproc://storage_thread_in_" + handler_id;
   const string thread_out_ep = "inproc://storage_thread_out_" + handler_id;
   threads_in.bind(thread_in_ep);
   threads_out.bind(thread_out_ep);

   // don't want to get signals when child threads die, as this interrupts the
   // zeromq poll loop and it's much easier to keep track of this stuff when
   // the poll loop simply has a short timeout and the threads are manually
   // collected. well... makes it easier to read, anyway.
   signal(SIGCHLD, SIG_IGN);

   // start worker threads - pre-fork them... maybe we'll try something more
   // complex if it becomes necessary...
   for (size_t i = 0; i < max_concurrency; ++i)
   {
      boost::shared_ptr<boost::thread> t
         (new boost::thread(thread_func,
                            boost::ref(conf), 
                            boost::ref(m_context),
                            boost::cref(m_dirty_list),
                            boost::ref(m_shutdown_requested),
                            thread_in_ep, thread_out_ep));
      
      threads.push_back(t);
   }
}

storage_worker::~storage_worker()
{
   // notify everyone that we want to shut down
   m_shutdown_requested = true;

   // collect all the threads
   BOOST_FOREACH(shared_ptr<boost::thread> thread, threads)
   {
      try 
      {
         thread->join();
      }
      catch (const std::exception &e) 
      {
         // don't want any exceptions escapting the destructor
         LOG_ERROR(boost::format("Error during thread shutdown: %1%") % e.what());
      }
   }
}

void 
storage_worker::operator()() {
   try {
      // temporary tile object
      tile_protocol tile;

      // time to next check for thread death
      bt::ptime next_check_time = bt::microsec_clock::local_time() + 
         bt::microseconds(CHECK_THREAD_DEATH_INTERVAL);

      while (true) {
         zmq::pollitem_t items [] = {
            { requests_in.socket(), 0, ZMQ_POLLIN, 0 },
            { threads_in.socket(),  0, ZMQ_POLLIN, 0 }
         };
      
         try {
            zmq::poll(items, 2, STORAGE_WORKER_POLL_TIMEOUT);
         } catch (const zmq::error_t &) {
            // error can be thrown in here due to interrupted system calls. this
            // can be because ctrl-C was pressed, or the process is being run 
            // via a debugger. i haven't yet seen a case where this error isn't
            // ignorable.
            continue;
         }
      
         // new items either get started, or put on the pending queue
         if (items[0].revents & ZMQ_POLLIN) 
         {
            requests_in >> tile;
        
            if (cur_concurrency < max_concurrency) 
            {
               threads_out << tile;
               ++cur_concurrency;
            } 
            else 
            {
               queued_requests.push_back(make_shared<tile_protocol>(tile));
            }
         }

         if (items[1].revents & ZMQ_POLLIN) 
         {
            threads_in >> tile;
            results_out << tile;

            if (!queued_requests.empty()) 
            {
               shared_ptr<tile_protocol> tile = queued_requests.front();
               threads_out << *tile;
               queued_requests.pop_front();
            }
            else
            {
               // no jobs to do - thread becomes idle.
               --cur_concurrency;
            }
         }

         if (bt::microsec_clock::local_time() > next_check_time)
         {
            BOOST_FOREACH(shared_ptr<boost::thread> thread, threads)
            {
               try 
               {
                  if (thread->timed_join(bt::microseconds(0)))
                  {
                     // thread has died - this isn't supposed to happen!
                     LOG_ERROR("Storage worker thread has died.");
                     
                     // remove this level of concurrency
                     --max_concurrency;

                     // check that there are any threads left to handle
                     // jobs. if not, this is a serious error and the
                     // system has to be shut down.
                     if (max_concurrency < 1)
                     {
                        LOG_ERROR("All storage worker threads have died.");
                        throw std::runtime_error("All storage worker threads have died.");
                     }
                  }
               }
               catch (const boost::thread_interrupted &e)
               {
                  // signal occurred - this is ignorable as the thread may
                  // or may not have died, but it can be collected next
                  // time around.
                  continue;
               }
            }

            // set up next interval
            next_check_time += bt::microseconds(CHECK_THREAD_DEATH_INTERVAL);
         }
      }
   } catch (...) {
   }
}

} // namespace rendermq
