/*------------------------------------------------------------------------------
 *
 *  Asynchronous tile renderering queue backend interface.
 *
 *  Author: matt.amos@mapquest.com
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

#include "zmq_backend.hpp"
#include "../zmq_utils.hpp"
#include "../tile_protocol.hpp"
#include "../proto/tile.pb.h"
#include "../zstream.hpp"
#include "../zstream_pbuf.hpp"
#include "distributed_queue_config.hpp"
#include "../logging/logger.hpp"

#include <iostream>
#include <list>
#include <map>
#include <iterator>
#include <limits>
#include <boost/tokenizer.hpp>
#include <boost/optional.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace pt = boost::property_tree;
using std::string;
using std::list;
using std::map;
using std::runtime_error;
using std::numeric_limits;
using boost::optional;
using boost::posix_time::ptime;
using boost::posix_time::microsec_clock;
using boost::posix_time::milliseconds;
using boost::format;
using boost::unordered_map;
using boost::bind;
namespace manip = zstream::manip;

// unless specified in the config file, the liveness (point at which to consider
// the broker dead) in seconds.
#define DEFAULT_LIVENESS (30)

// unless specified in the config file, the timeout to use on the worker poll
// loop, in seconds. (note: 10 sec at the moment)
#define DEFAULT_POLL_TIMEOUT (10)

// if not specified in the config file, how long to wait for a response from
// the broker after requesting a job.
#define DEFAULT_BROKER_TIMEOUT (30)

// number of seconds between the subscription sockets being torn down and
// re-used. this can be set very long, as this appears to be a problem which
// builds up over the course of several days.
#define DEFAULT_RESUBSCRIBE_INTERVAL (3600)

// number of repeats on the consistent hash "clock". 
// TODO: figure out what a good value is...
#define CONSISTENT_HASH_NUM_REPEATS (100)

namespace 
{

dqueue::runner_backend *create_zmq_runner(const pt::ptree &pt, zmq::context_t &ctx) 
{
   return new dqueue::zmq_backend_handler(pt, ctx);
}

dqueue::supervisor_backend *create_zmq_supervisor(const pt::ptree &pt)
{
   return new dqueue::zmq_backend_worker(pt);
}

// register the ZMQ backend
const bool registered = register_backend("zmq", create_zmq_runner, create_zmq_supervisor);

} // anonymous namespace

namespace dqueue {

zmq_backend_common::zmq_backend_common(zmq::context_t &ctx) 
   : m_ctx(ctx),
     broker_req(m_ctx),
     broker_sub(new zstream::socket::sub(m_ctx)) {
}

zmq_backend_common::~zmq_backend_common() {
}

void
zmq_backend_common::setup(const list<string> &req_eps, 
                          const list<string> &sub_eps) {
   // perform some sanity checks
   if (req_eps.size() != sub_eps.size()) {
      throw runtime_error((format("Request endpoints size (%1%) doesn't match "
                                  "subscription endpoints size (%2%), which is "
                                  "so highly suspicious that it's an error.")
                           % req_eps.size() % sub_eps.size()).str());
   }

   LOG_DEBUG("Setting up endpoints...");

   // connect up the endpoints
   for (list<string>::const_iterator itr = req_eps.begin(); itr != req_eps.end(); ++itr) {
      LOG_DEBUG(boost::format("  XREQ %1%") % *itr);
      try {
         broker_req.connect(*itr);
      } catch (const zmq::error_t &err) {
         throw broker_error("Cannot connect XREQ socket.", err);
      }
   }    
   for (list<string>::const_iterator itr = sub_eps.begin(); itr != sub_eps.end(); ++itr) {
      LOG_DEBUG(boost::format("  SUB %1%") % *itr);
      try {
         broker_sub->connect(*itr);
      } catch (const zmq::error_t &err) {
         throw broker_error("Cannot connect SUB socket.", err);
      }
   }

   // record for posterity
   m_config_sub_eps = sub_eps;
}

void zmq_backend_common::resubscribe()
{
   // open a new socket. we will connect this first before 
   // replacing the old one.
   boost::scoped_ptr<zstream::socket::sub> sock(new zstream::socket::sub(m_ctx));

   // reconnect to the same endpoints as last time.
   BOOST_FOREACH(const std::string &ep, m_config_sub_eps)
   {
      LOG_DEBUG(boost::format("  SUB %1%") % ep);
      try 
      {
         sock->connect(ep);
      } 
      catch (const zmq::error_t &err) 
      {
         throw broker_error("Cannot connect SUB socket.", err);
      }
   }

   // connection successful, replace old socket with new (old 
   // socket will be cleaned up when the pointer goes out of
   // scope).
   broker_sub.swap(sock);
}
  
/*************************************************************
 * worker functions
 */

struct zmq_backend_worker::task_communicator {
   /* use this to track the state of the communicator, as it can 
    * get a little bit complicated as it receives messages and
    * resends them and so forth.
    *
    * the state transition diagram looks a bit like:
    *
    *         +--------------------------------------------------+
    *         v                                                  |
    *     +--------+                                             |
    *     |  IDLE  |---- get job request  --------+              |
    *     +--------+     from worker code.        |              |
    *                                             v              |
    *                                        ==================  |
    *                                        =  any brokers   =  |
    *                                        = available with =  |
    *                                        =     jobs?      =  |
    *                                        ==================  |
    *     +--------+                            N |    Y |  ^    |
    *     |  WAIT  | <----------------------------+      |  |    |
    *     +--------+ ----+                               |  |    |
    *                    broker announces job.           |  |    |
    *                    |                               |  |    |
    *     +--------+ <---+                               |  |    |
    *     |  TRY   | <-------------- send job request ---+  |    |
    *     |        |                 to best broker.        |    |
    *     +--------+ -- timeout                             |    |
    *          |        or no job --------------------------+    |
    *          |        available                                |
    *     job response                                           |
    *     from broker                                            |
    *          |                                                 |
    *          v                                                 |
    *     +--------+                                             |
    *     |  PROC  |---- job response from ----------------------+
    *     +--------+     worker code.
    *
    * there's also an almost-separate event queue in that each of
    * these states (except WAIT), when it gets a broker announcement,
    * uses it to update the internal state of which brokers have
    * available jobs and at which priority.
    */
   enum communicator_state {
      state_idle,              /* when the client worker code hasn't
                                * asked for a job yet. */
      state_waiting_for_job,   /* when the client worker code has 
                                * asked for a job, but one hasn't been
                                * found yet. */
      state_trying_to_get_job, /* client code asked for a job and a 
                                * request to a broker has been sent 
                                * out */
      state_job_processing,    /* client code has a job and is working 
                                * on it. */
   };

   task_communicator(zmq::context_t &ctx, 
                     long p_timeout, long b_timeout,
                     bool &sh_req, const string &wrk_id) 
      : common(ctx), inproc_req(ctx), poll_timeout(p_timeout), 
        broker_timeout(b_timeout), shutdown_requested(sh_req), 
        state(state_idle), worker_id(wrk_id) {
   }

   void operator()() {
      LOG_DEBUG("Starting communication thread ...");

      // make unique so that many can coexist in one process for testing
      // purposes.
      inproc_req.connect("inproc://communication-" + worker_id);

      while (!shutdown_requested) {
         zmq::pollitem_t items [] = { 
            // point-to-point communication between the worker and broker.
            { common.broker_req.socket(), 0, ZMQ_POLLIN, 0 },
            // collects the brokers' announcements of the ready jobs.
            { common.broker_sub->socket(), 0, ZMQ_POLLIN, 0 },
            // notifications of completed jobs from the worker
            { inproc_req.socket(), 0, ZMQ_POLLIN, 0 },
         };

         zmq::poll(items, 3, poll_timeout);

         // first check that the broker that we were trying to get a job
         // from hasn't died or otherwise timed out.
         if ((state == state_trying_to_get_job) &&
             (get_job_retry_time < microsec_clock::universal_time())) {
            
            if (current_broker) {
               LOG_WARNING(boost::format("Dropped job request to current broker "
                                         "(\"%1%\"), assuming it has died.") 
                           % current_broker.get());
                        
               // try not to go back to the same broker again...
               brokers_with_jobs.erase(current_broker.get());

            } else {
               LOG_ERROR("Dropped job request to unknown current broker.");
            }

            try_to_get_job();
         }

         // responses from the broker
         if (items[0].revents & ZMQ_POLLIN) {
            list<string> headers;
            string response;

            manip::routing_headers routing_headers(headers);
            common.broker_req >> routing_headers >> response;

            // if we get a stray message just ignore it...
            if ((response.compare("JOB") == 0) && common.broker_req.has_more()) {
               // get job and send to inproc.
               rendermq::tile_protocol tile;
               common.broker_req >> tile;
          
               // check that we're looking for a job and looking for one 
               // from this particular broker...
               if ((state == state_trying_to_get_job) &&
                   (current_broker == headers.front())) {
                  inproc_req << tile;
                  current_broker = headers.front();
            
                  LOG_INFO(boost::format("Got job (%1%) from broker (\"%2%\").")
                           % tile % current_broker.get());

                  // next state is processing
                  state = state_job_processing;

               } else {
                  LOG_WARNING(boost::format("Unexpected job offer from broker %1%.") 
                              % headers.front());
               }
            
            } else {
               // no jobs... remove from list and try again.
               brokers_with_jobs.erase(headers.front());
               // if worker wants job...
               if (state == state_trying_to_get_job) {
                  try_to_get_job();
               }
            }

            // job availability announcements from broker
         } else if (items[1].revents & ZMQ_POLLIN) {
            string broker_id, msg;
            uint32_t max_priority;
            uint64_t qsize;

            (*common.broker_sub) >> broker_id >> msg >> max_priority >> qsize;
            brokers_with_jobs[broker_id] = broker_status(max_priority, qsize);

            // if we are waiting for a job, then try and grab this one immediately
            if (state == state_waiting_for_job) {
               try_to_get_job();
            }

            // worker thread replies to be routed back to broker
         } else if (items[2].revents & ZMQ_POLLIN) {
            string dummy;
            inproc_req >> dummy;
            if (inproc_req.has_more()) {
               // this means it's finished and it needs to notify
               rendermq::tile_protocol tile;
               inproc_req >> tile;
               if (state == state_job_processing) {
                  if (current_broker) {
                     common.broker_req.to(current_broker.get()) 
                        << manip::more << "RESULT"
                        << tile;
              
                     current_broker = boost::none;
                     state = state_idle;

                  } else {
                     // the state machine implies this can never happen, as
                     // current_broker is set anywhere we accept a job and
                     // no other processing should be done, but never say 
                     // never...
                     LOG_DEBUG("worker returned a job, but there's no-one to send it to.");
                  }
               } else {
                  // the state machine implies this can never happen, as jobs
                  // are only sent to the worker where we enter the job 
                  // processing state.
                  LOG_DEBUG("worker returned a job, but the state is not job_processing.");
               }
            } else {
               // this means a request for a job. if there's a broker which has 
               // advertised that it has jobs, go talk to it.
               if (state == state_idle) {
                  try_to_get_job();

               } else {
                  // the state machine implies this can never happen, as job 
                  // requests can only come from the worker when the state is
                  // idle.
                  LOG_DEBUG("worker requested a job, but the state is not idle.");
               }
            }
         }
      }
   }

   boost::optional<string> highest_priority_broker() const
   {
      typedef unordered_map<string, broker_status>::value_type status_t;
      boost::optional<string> broker;
      broker_status best;
      
      BOOST_FOREACH(status_t status, brokers_with_jobs)
      {
         if (status.second > best) 
         {
            best = status.second;
            broker = status.first;
         }
      }

      return broker;
   }

   /* examines the state of what's known about the brokers' queues to
    * move to the appropriate state for getting a job. this might be
    * immediately available, or it might mean waiting for a broker to
    * announce that a new job is available.
    */
   void try_to_get_job() {
      current_broker = highest_priority_broker();

      if (current_broker) {
         common.broker_req.to(current_broker.get()) << "GET_JOB";
         state = state_trying_to_get_job;
         // set up a time after which this worker will give up trying to 
         // get a job from the current broker, assuming it has died, and
         // try a different one instead.
         get_job_retry_time = microsec_clock::universal_time() + milliseconds(broker_timeout);

      } else {
         // otherwise switch state back to waiting so that
         // an announce might trigger another attempt.
         state = state_waiting_for_job;
      }
   }

   zmq_backend_common common;
   zstream::socket::pair inproc_req;

   // poll and broker timeouts. the poll loop timeout is in microseconds, 
   // the broker timeout in milliseconds.
   long poll_timeout, broker_timeout;

   // whether we've been asked to shutdown this thread.
   bool &shutdown_requested;

   // what stage of processing is this communicator in?
   communicator_state state;

   // this worker's ID
   const string &worker_id;

   // whether we're currently polling a particular worker for a job
   boost::optional<string> current_broker;

   /* workers keep track of the status of the brokers to fairly
    * attempt to get the highest priority job. jobs are ordered by
    * priority on the broker and, between jobs with the same 
    * priority, by time. therefore the fairest way to take jobs is
    * by priority first, then by queue length (as a proxy for time).
    */
   struct broker_status 
   {
      // set up to be the worst possible (in fact impossible) status
      // such that all other statuses are greater than this one.
      broker_status()
         : m_max_priority(std::numeric_limits<int>::min()),
           m_queue_length(std::numeric_limits<int>::min()) {}

      broker_status(int max_priority, int queue_length)
         : m_max_priority(max_priority), m_queue_length(queue_length) {}

      // ordering is by priority first, then by queue length. this 
      // is for some form of fairness...
      bool operator>(const broker_status &other) const
      {
         return (m_max_priority > other.m_max_priority) ||
            ((m_max_priority == other.m_max_priority) &&
             (m_queue_length > other.m_queue_length));
      }

      // the maximum priority of any task on this broker - i.e: the
      // task at the head of the queue.
      int m_max_priority;

      // the length of the queue on this broker, used as a balancing
      // measure and as a proxy for time to try and be fair.
      int m_queue_length;
   };

   // map the identities of the brokers into their respective statuses.
   unordered_map<string, broker_status> brokers_with_jobs;

   // time at which to give up on a (presumably) dead broker and retry
   ptime get_job_retry_time;
};

zmq_backend_worker::zmq_backend_worker(const pt::ptree &pt) 
   : owns_context(true),
     context(new zmq::context_t(1)),
     inproc_rep(*context),
     shutdown_requested(false) {
   setup(pt);
}

zmq_backend_worker::zmq_backend_worker(const pt::ptree &pt, zmq::context_t &ctx)
   : owns_context(false),
     context(&ctx),
     inproc_rep(*context),
     shutdown_requested(false) {
   setup(pt);
}

void 
zmq_backend_worker::setup(const pt::ptree &pt) {
   poll_timeout = long(pt.get<double>("worker.poll_timeout", DEFAULT_POLL_TIMEOUT) * 1000000);
   long broker_timeout = long(pt.get<double>("worker.broker_timeout", DEFAULT_BROKER_TIMEOUT) * 1000);

   boost::optional<std::string> config_worker_id = pt.get_optional<std::string>("worker.id");
   if (config_worker_id) {
      worker_id = config_worker_id.get();
   } else {
      worker_id = dqueue::util::make_uuid();
   }

   // make unique so it can be used in tests...
   inproc_rep.bind("inproc://communication-" + worker_id);

   communicator.reset(new task_communicator(*context, poll_timeout, 
                                            broker_timeout, shutdown_requested,
                                            worker_id));
   comm_thread.reset(new boost::thread(boost::ref(*communicator)));

   // set the identity on the REQ socket to support identity routing. this
   // isn't necessary for the SUB socket, as routing isn't needed on that one.
   communicator->common.broker_req.set_identity(worker_id);

   conf::common brokers(pt);
   communicator->common.setup(brokers.all_out_req(), brokers.all_out_sub());
}

zmq_backend_worker::~zmq_backend_worker() /* no-throw */ {
   shutdown_requested = true;
   comm_thread->join();
}

job_t
zmq_backend_worker::get_job() {
   job_t job;
   inproc_rep << "";
   inproc_rep >> job;
   return job;
}

void
zmq_backend_worker::notify(const job_t &job) {
   inproc_rep << manip::more << "" << job;
}

/*************************************************************
 * handler functions
 */

zmq_backend_handler::zmq_backend_handler(const pt::ptree &pt, zmq::context_t &ctx) 
   : common(ctx),
     router(CONSISTENT_HASH_NUM_REPEATS)
{
   double config_liveness = pt.get<double>("zmq.liveness_time", DEFAULT_LIVENESS);
   liveness_time = milliseconds(long(config_liveness * 1000));

   double config_resub_interval = pt.get<double>("zmq.resubscribe_interval", DEFAULT_RESUBSCRIBE_INTERVAL);
   m_sub_reconnect_interval = milliseconds(long(config_resub_interval * 1000));

   optional<double> config_settle = pt.get_optional<double>("zmq.settle_time");
   if (config_settle)
   {
      m_settle_time = microsec_clock::local_time() + milliseconds(long(config_settle.get() * 1000));
   }
  
   conf::common brokers(pt);
   common.setup(brokers.all_in_req(), brokers.all_in_sub());

   // set the next reconnect interval after connecting the common stuff
   m_next_sub_reconnect = microsec_clock::local_time() + m_sub_reconnect_interval;
}

zmq_backend_handler::~zmq_backend_handler() /* no-throw */ {
}

void
zmq_backend_handler::update_live_brokers() {
   ptime now = microsec_clock::local_time();

   // if timer has expired, reconnect the broker subscription sockets.
   // ideally, this shouldn't be necessary, but it seems that it's 
   // possible for the broker to believe the connection is closed and
   // the handler to think it's open, but the broker just isn't saying
   // anything. in this situation it's possible for the brokers to 
   // become isolated, reducing the effectivenes of a distributed 
   // system.
   if (now > m_next_sub_reconnect)
   {
      // move next occurrance into the future.
      while (now > m_next_sub_reconnect)
      {
         m_next_sub_reconnect += m_sub_reconnect_interval;
      }
      
      // reset the subscription connections
      common.resubscribe();
   }

   for (unordered_map<string, heartbeat>::iterator itr = heartbeats.begin();
        itr != heartbeats.end(); ++itr) {
      heartbeat &hb = itr->second;
      const bool is_live = now - hb.time < liveness_time;

      if (is_live && !hb.is_live) {
         // this broker wasn't alive, but has now sprung back to life and needs
         // to be re-added to the consistent hash rotation.
         router.insert(itr->first);
         hb.is_live = true;
         LOG_INFO(boost::format("Broker `%1%' has sprung to life.") % itr->first);
      }

      if (!is_live && hb.is_live) {
         // this broker has died and needs to be removed from the rotation.
         router.erase(itr->first);
         hb.is_live = false;
         LOG_WARNING(boost::format("Broker `%1% appears to have died.") % itr->first);
      }
   }
}

void 
zmq_backend_handler::send(const job_t &job) {
   // if the queue is still settling, then don't send any jobs - throw
   // an error instead.
   if (settle_check())
   {
      throw broker_error("still settling.");
   }
      
   // update the list of brokers which are known to be live.
   update_live_brokers();

   // use the consistent hash to find which of these will handle the job.
   optional<string> broker_id = router.lookup(job);

   if (broker_id) {
      // send job to specified broker.
      LOG_DEBUG(boost::format("sending job to `%1%'.") % broker_id.get());

      common.broker_req.to(broker_id.get()) << job;

   } else {
      // uh-oh... signal error. this should be caught by the handler and
      // converted into an appropriate error.
      throw broker_error("no brokers available to send jobs to!");
   }
}

int
zmq_backend_handler::num_pollitems() {
   return 2; // req and sub sockets.
}

void 
zmq_backend_handler::fill_pollitems(zmq::pollitem_t *items) {
   items[0].socket = common.broker_req.socket();
   items[0].fd = 0;
   items[0].events = ZMQ_POLLIN;
   items[1].socket = common.broker_sub->socket();
   items[1].fd = 0;
   items[1].events = ZMQ_POLLIN;
}

zmq_backend_handler::heartbeat::heartbeat() 
   : time(), // note: this will be an invalid time, but updated in update_heartbeat.
     queue_size(0),
     is_live(false) {
}

void 
zmq_backend_handler::update_heartbeat(const string &broker_id, uint64_t qsize) {
   LOG_FINER(boost::format("HEARTBEAT! %1% is alive...") % broker_id);
   heartbeat &hb = heartbeats[broker_id];
   hb.time = microsec_clock::local_time();
   hb.queue_size = qsize;
}

bool
zmq_backend_handler::handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs) {
   bool have_new_jobs = false;

   // subscription socket - receive broker heartbeats
   if (items[1].revents & ZMQ_POLLIN) {
      // update internal structure to show recency of broker heartbeat.
      // note that the broker ID being sent over the wire will be the broker's
      // *XREP* socket ID, not the PUB one...
      std::string msg;
      uint64_t qsize;
      (*common.broker_sub) >> msg >> qsize;
      update_heartbeat(msg, qsize);
   }

   if (items[0].revents & ZMQ_POLLIN) {
      job_t job;
      common.broker_req >> manip::ignore_routing_headers
                        >> job;

      jobs.push_back(job);

      have_new_jobs = true;
   }

   return have_new_jobs;
}

size_t 
zmq_backend_handler::queue_length() const {
   // if the queue is settling, then send back the largest queue
   // length that is possible - i.e: as close to infinity as we
   // can get. in conjunction with queue management options, this
   // should mean that no jobs get sent to the queue.
   if (settle_check())
   {
      return numeric_limits<size_t>::max();
   }

   size_t count = 0, qsize = 0;

   for (unordered_map<string, heartbeat>::const_iterator itr = heartbeats.begin();
        itr != heartbeats.end(); ++itr) {
      const heartbeat &hb = itr->second;
      if (hb.is_live) {
         ++count;
         qsize += hb.queue_size;
      }
   }

   return (count == 0) ? 0 : qsize / count;
}

bool
zmq_backend_handler::settle_check() const
{
   // this is a const version of the routine, so that it
   // can be called from other const methods.
   return m_settle_time && 
      (m_settle_time.get() >= microsec_clock::local_time());
}

bool
zmq_backend_handler::settle_check() 
{
   if (m_settle_time)
   {
      if (m_settle_time.get() < microsec_clock::local_time())
      {
         // settling has finished
         update_live_brokers();

         size_t num_alive = 0;
         for (unordered_map<string, heartbeat>::iterator itr = heartbeats.begin();
              itr != heartbeats.end(); ++itr) 
         {
            const heartbeat &hb = itr->second;
            if (hb.is_live) 
            {
               ++num_alive;
            }
         }

         // unset this flag
         m_settle_time.reset();

         LOG_INFO(boost::format("Settled. Found %1% live brokers.") % num_alive);

         return false;
      }
      else
      {
         // still settling... need more time to find all the brokers.
         return true;
      }
   }
   
   // settling flag has been unset, so settling is done.
   return false;
}

/*
void
zmq_backend_handler::settle(list<job_t> &jobs)
{
   // settle for the liveness time - should give plenty of time 
   // for brokers to report in...
   ptime settle_ends = microsec_clock::local_time() + liveness_time;
   LOG_INFO(boost::format("Settling until %1%...") % settle_ends);

   while (settle_ends > microsec_clock::local_time()) 
   {
      // broker polling items
      zmq::pollitem_t items [] = {
         { NULL, 0, ZMQ_POLLIN, 0 },
         { NULL, 0, ZMQ_POLLIN, 0 },
      };
      fill_pollitems(&items[0]);

      try {
         zmq::poll(&items[0], 2, liveness_time.total_milliseconds());
      } catch (const zmq::error_t &) {
         // ignore errors while settling...
         continue;
      }

      handle_pollitems(&items[0], jobs);
   }

   // update how many brokers we think are alive.
   update_live_brokers();

   size_t num_alive = 0;
   for (unordered_map<string, heartbeat>::iterator itr = heartbeats.begin();
        itr != heartbeats.end(); ++itr) 
   {
      const heartbeat &hb = itr->second;
      if (hb.is_live) 
      {
         ++num_alive;
      }
   }
      
   LOG_INFO(boost::format("Settled. Found %1% live brokers.") % num_alive);
}
*/

}
