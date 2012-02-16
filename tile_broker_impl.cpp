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

#include "tile_broker_impl.hpp"

#include "task_queue.hpp"
#include "zstream.hpp"
#include "zmq_utils.hpp"
#include "tile_protocol.hpp"
#include "zstream_pbuf.hpp"
#include "dqueue/distributed_queue_config.hpp"
#include "logging/logger.hpp"

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <boost/array.hpp>
#include <boost/tokenizer.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <map>
#include <queue>
#include <iostream>
#include <sstream>
#include "storage/meta_tile.hpp"

using std::string;
using std::list;
using std::map;
using std::string;
using std::ostringstream;
using boost::format;
namespace manip = zstream::manip;
namespace pt = boost::property_tree;

// the amount of time that a task has to be in the queue without being
// completed before the worker running it is considered to be dead and
// the task assigned to it should be resubmitted to the queue to go to
// a different worker.
#define DEFAULT_ZOMBIE_TIME (300)

namespace {

/* thread which runs to send messages to the main thread reminding it
 * to heartbeat. this, bizarrely, makes things simpler in 0MQ, since 
 * the main thread no longer has to deal with timers and interleaving
 * them into the message loop - effectively this thread converts timer
 * expiry into a 0MQ message.
 */
struct task_monitor
{
  explicit task_monitor(zmq::context_t & ctx, 
                        unsigned int beat_interval, 
                        unsigned int resub_interval,
                        bool &sd_req, 
                        const string &name)
    : ctx_(ctx), heartbeat_interval(beat_interval),
      resubmit_interval(resub_interval),
      shutdown_requested(sd_req), broker_name(name) {}
    
    void operator() ()
    {
       LOG_DEBUG(boost::format("Starting heartbeat thread ... (%1%, %2%)") 
                 % heartbeat_interval % resubmit_interval);

        zstream::socket::req cmd(ctx_);

        // counters to keep track of which event(s) should be processed at
        // each moment in time.
        unsigned int next_heartbeat = 0, next_resubmit = resubmit_interval;

        // use the broker name to disambiguate the monitor addresses if there
        // is more than one broker running in this process, as can happen when
        // we're testing.
        cmd.connect("inproc://monitor-" + broker_name); 

        while (!shutdown_requested) {
          string reply;

          if (next_heartbeat == 0) {
            cmd << "HEARTBEAT";
            cmd >> reply; // which we just discard...
            next_heartbeat = heartbeat_interval;
          }

          if (next_resubmit == 0) {
            cmd << "RESUBMIT ZOMBIE TASKS";
            cmd >> reply;
            next_resubmit = resubmit_interval;
          }

          unsigned int sleep_interval = std::min(next_heartbeat, next_resubmit);
          next_heartbeat -= sleep_interval;
          next_resubmit -= sleep_interval;
          sleep(sleep_interval);
        }
    }
    
    zmq::context_t & ctx_;  
  unsigned int heartbeat_interval, resubmit_interval;
  bool &shutdown_requested;
  string broker_name;
};

void send_tile_to_listeners(rendermq::task_queue &queue,
                            zstream::socket::xrep &frontend_rep,
                            rendermq::tile_protocol tile_from_worker,
                            const std::string &worker_address) {
  typedef rendermq::task::iterator task_iterator;
  typedef std::pair<task_iterator, task_iterator> task_range;
  typedef std::pair<string::const_iterator, string::const_iterator> string_const_range;

  boost::optional<const rendermq::task &> t = queue.get(tile_from_worker);

  if (t) {
    task_range range = (*t).subscribers();
    for (task_iterator itr = range.first; itr != range.second; ++itr) {
       LOG_FINER(boost::format("SUB %1% addr size: %2%") % itr->first % itr->second.size());
       rendermq::tile_protocol tile_for_handler(itr->first);
       LOG_FINER(boost::format("with tile = %1%") % tile_for_handler);
                
      if ((tile_for_handler.status != rendermq::cmdDirty) &&
          (tile_for_handler.status != rendermq::cmdRenderBulk)) 
      {
         tile_for_handler.status = tile_from_worker.status;
         tile_for_handler.last_modified = tile_from_worker.last_modified;

         if (tile_from_worker.status != rendermq::cmdNotDone) 
         {
            rendermq::metatile_reader reader(tile_from_worker.data(), itr->first.format);
            string_const_range range = reader.get(tile_for_handler.x, tile_for_handler.y);
            // TODO: add error handling when range is zero?
            tile_for_handler.set_data(string(range.first,range.second));
         }
         frontend_rep.to(itr->second) << tile_for_handler;
      }
    }
    // erase task
    queue.erase(tile_from_worker);
  }
}

} // anonymous namespace

namespace rendermq {

struct broker_impl::pimpl {
  pimpl(const pt::ptree &config, 
        const string &name,
        zmq::context_t &ctx) 
    : context(ctx),
      frontend_rep(context), frontend_pub(context), 
      backend_rep(context), backend_pub(context),
      monitor(context),
      heartbeat_interval(config.get<unsigned int>("zmq.heartbeat_time")),
      resubmit_interval(config.get<unsigned int>("zmq.resubmit_interval", heartbeat_interval)),
      zombie_time(config.get<unsigned int>("zmq.zombie_time", DEFAULT_ZOMBIE_TIME)),
      shutdown_requested(false),
      broker_name(name) {
  }

  void publish_availability() {
    boost::optional<const task &> t = queue.front();

    if (t) {
      uint32_t priority = t->priority();
      uint64_t unprocessed = queue.count_unprocessed();

      LOG_FINER(boost::format("Publish: %1% jobs available at priority %2%") % unprocessed % priority);

      // send own XREQ address followed by message about task availability, the highest
      // priority item in the queue and the number of unprocessed items in the queue.
      backend_pub 
        << manip::more << backend_rep.identity() 
        << manip::more << "JOBS AVAILABLE"
        << manip::more << priority << unprocessed;
    }
  }

  // the external context to use for the broker
  zmq::context_t &context;

  // frontend (handler) communication sockets
  zstream::socket::xrep frontend_rep;
  zstream::socket::pub frontend_pub;

  // backend (worker) communication sockets
  zstream::socket::xrep backend_rep;
  zstream::socket::pub backend_pub;

  // control socket for sending the running broker commands
  zstream::socket::rep monitor;

  // number of seconds between heartbeats
  unsigned int heartbeat_interval;

  // number of seconds between resubmits of zombie tasks (tasks
  // which haven't been completed within some interval and are
  // likely to be dead workers).
  unsigned int resubmit_interval;

  // number of seconds before a worker is considered dead, and
  // tasks assigned to it are considered zombies.
  unsigned int zombie_time;

  // flag to shut down the heartbeat thread cleanly.
  bool shutdown_requested;

  // queue of jobs being processed or waiting to be processed
  rendermq::task_queue queue;

  // name of the broker.
  string broker_name;
};

broker_impl::broker_impl(const pt::ptree &config, 
                         const string &broker_name, 
                         zmq::context_t &ctx) 
  : impl(new pimpl(config, broker_name, ctx)) {

  dqueue::conf::common dconf(config);
  map<string,dqueue::conf::broker>::iterator self = dconf.brokers.find(broker_name);
  if (self == dconf.brokers.end()) {
    ostringstream ostr;
    ostr << "Broker name `" << broker_name << "' isn't present in config file.";
    throw std::runtime_error(ostr.str());
  }
  
  string frontend_req_addr = self->second.in_req;
  string frontend_pub_addr = self->second.in_sub;
  string backend_req_addr = self->second.out_req;
  string backend_pub_addr = self->second.out_sub;
  string monitor_addr = self->second.monitor;
  string frontend_identity = self->second.in_identity.get_value_or(dqueue::util::make_uuid());
  string backend_identity = self->second.out_identity.get_value_or(dqueue::util::make_uuid());
  
  // init ZMQ sockets
  impl->frontend_rep.set_identity(frontend_identity);
  impl->frontend_rep.bind(frontend_req_addr.c_str());
  impl->frontend_pub.bind(frontend_pub_addr.c_str());
  
  impl->backend_rep.set_identity(backend_identity);
  impl->backend_rep.bind(backend_req_addr.c_str());
  impl->backend_pub.bind(backend_pub_addr.c_str());
  
  impl->monitor.bind(monitor_addr); // external monitor
  // needs to have the broker name, as for testing we'll sometimes have
  // multiple brokers running inside the same process.
  impl->monitor.bind("inproc://monitor-" + broker_name); // internal monitor thread 
}

broker_impl::~broker_impl() {
}

void 
broker_impl::operator()() {    
  // start monitor/heartbeat thread
  task_monitor mon(impl->context, 
                   impl->heartbeat_interval, 
                   impl->resubmit_interval,
                   impl->shutdown_requested,
                   impl->broker_name);
  boost::thread t(mon);
  
  while (true) {
    //  Initialize poll set
    zmq::pollitem_t items [] = {
      // Always poll for worker activity on backend
      { impl->backend_rep.socket(),  0, ZMQ_POLLIN, 0 },
      // Always poll front-end
      { impl->frontend_rep.socket(), 0, ZMQ_POLLIN, 0 },
      // Monitoring socket
      { impl->monitor.socket(), 0, ZMQ_POLLIN, 0 },
    };
    
    zmq::poll (&items [0], 3, -1);
    
    //  Handle worker activity on backend
    if (items [0].revents & ZMQ_POLLIN) {
      list<string> worker_addresses;
      string command;
      
      // message parts are worker, client addresses then the returned metatile.
      manip::routing_headers headers(worker_addresses);
      impl->backend_rep >> headers >> command;
      LOG_FINER(boost::format("Message from `%1%': %2%") % worker_addresses.front() % command);

      if (command.compare("RESULT") == 0) { 
        tile_protocol meta;
        impl->backend_rep >> meta;
        send_tile_to_listeners(impl->queue, impl->frontend_rep, meta, worker_addresses.front());
      }
      
      if (command.compare("GET_JOB") == 0) {
        boost::optional<const task &> t = impl->queue.front();
        if (t) {
          tile_protocol proto = static_cast<tile_protocol>(*t);
          impl->backend_rep.to(worker_addresses) << manip::more << "JOB" << proto;
          impl->queue.set_processed(proto);
          
        } else {
          impl->backend_rep.to(worker_addresses) << "NO JOBS";
        }
        // TODO: do we need the "optimisation" of sending back whether there are
        // any jobs when the worker gives us back a complete job?
      }
    }
    
    // frontend communications with the handlers
    if (items [1].revents & ZMQ_POLLIN) {            
      list<string> client_addresses;
      tile_protocol tile;
      
      zstream::manip::routing_headers headers(client_addresses);
      impl->frontend_rep >> headers >> tile;
      
      int priority = 100;
      if (tile.status == cmdRenderBulk ) priority = 0;
      else if (tile.status == cmdDirty) priority = 50;
      else if (tile.status == cmdRenderPrio) priority = 150;

      LOG_FINER(boost::format("Tile request: %1% priority=%2%") % tile % priority);

      // take a look at the highest priority task in the queue before we add this one.
      boost::optional<const task &> front_task = impl->queue.front();
      
      impl->queue.push(tile, client_addresses.front(), priority);
      
      // we send out a notification to all listening workers if the priority of the 
      // highest priority item in the queue has changed.
      if ((!front_task) || (front_task->priority() < priority)) {
        impl->publish_availability();
      }
    }
    
    // monitor
    if (items [2].revents & ZMQ_POLLIN) {
      string str;
      impl->monitor >> str;
      
      if (str.compare("CLEAR TASK QUEUE") == 0) {
        impl->queue.clear();
        impl->monitor << str;

      } else if (str.compare("RESUBMIT ZOMBIE TASKS") == 0) {
        impl->queue.resubmit_older_than(impl->zombie_time); // older then 30 sec
        impl->monitor << str;

      } else if (str.compare("STATS") == 0) {
        size_t size = impl->queue.size();
        size_t unprocessed = impl->queue.count_unprocessed();
        int priority = impl->queue.front() ? impl->queue.front()->priority() : -1;

        string stats = (boost::format("num_tasks=%d num_unprocessed=%d highest_priority=%d") 
                        % size % unprocessed % priority).str();
        impl->monitor << stats;
        
      } else if (str.compare("HEARTBEAT") == 0) {
        // send frontends a queue count, so they know how busy the queues
        // are. this should allow them to make decisions about whether to 
        // send clients old tiles or not.
        impl->frontend_pub 
          << manip::more << impl->frontend_rep.identity()
          << uint64_t(impl->queue.count_unprocessed());

        // publish availability information to the workers, so that they 
        // can claim jobs if they want to.
        impl->publish_availability();
        impl->monitor << str;
        
      } else if (str.compare("SHUTDOWN") == 0) {
        impl->monitor << str;
        break;
        
      } else {
        impl->monitor << "UNKNOWN";
      }
    }
  }

  // attempt to shut down somewhat cleanly
  impl->shutdown_requested = true;
  t.join();
  LOG_DEBUG(boost::format("Shutting down with %1% jobs still in the queue.") % impl->queue.size());
}

} // namespace rendermq

