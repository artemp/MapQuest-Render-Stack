/*------------------------------------------------------------------------------
 *
 *  Asynchronous tile renderering queue backend interface.
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

#ifndef ZMQ_BACKEND_HPP
#define ZMQ_BACKEND_HPP

#include <zmq.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include "backend.hpp"
#include "consistent_hash.hpp"
#include "../zstream.hpp"

namespace dqueue {

class zmq_backend_common {
public:
   // construct from context
   zmq_backend_common(zmq::context_t &ctx);

   ~zmq_backend_common();

   /* sets up the two sockets below according to the config file, with
    * the given parameter names for the list of REQ and SUB endpoints.
    *
    * this is separated from the constructor so that other classes can
    * do intermediate setup before the sockets are connected, such as
    * setting identities.
    */
   void setup(const std::list<std::string> &config_req_eps, 
              const std::list<std::string> &config_sub_eps);
  
   /* closes, reopens and reconnects the subscription sockets.
    */
   void resubscribe();

   /* reference to the zeromq context so that we can replace sockets
    * when reconnects are needed. 
    */
   zmq::context_t &m_ctx;

   /* we have 2 sockets for (all) the brokers:
    *   - a SUB socket for listening to the brokers' heartbeats (on the 
    *     worker: brokers' job announcements).
    *   - an XREP socket for routing requests to/from the brokers.
    *
    * note that we only need 2 sockets because zmq sockets can be connected
    * to multiple endpoints with (generally) fair-queued semantics on the
    * received messages.
    */
   zstream::socket::xrep broker_req;
   boost::scoped_ptr<zstream::socket::sub> broker_sub;

   /* the list of subscription endpoints that we last connected to.
    */
   std::list<std::string> m_config_sub_eps;
};

/**
 * 0MQ-based backend for the queue, from the handler's point of view. The 
 * worker connection is handled by a different class, zmq_backend_worker.
 *
 * Note that this requires a running broker process.
 */
class zmq_backend_handler : public runner_backend {
public:
   /* sets up the backend sharing a context. this is necessary for 0MQ to poll
    * properly when used in the handler with other 0MQ sockets.
    */
   zmq_backend_handler(const boost::property_tree::ptree &pt, zmq::context_t &context);
   ~zmq_backend_handler(); // no-throw

   void send(const job_t &job);

   int num_pollitems();
   void fill_pollitems(zmq::pollitem_t *items);
   bool handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs);

   size_t queue_length() const;

private:
   // common stuff shared between brokers
   zmq_backend_common common;

   // consistent hash structure to route jobs to brokers
   rendermq::consistent_hash<job_t, std::string> router;

   // time used to determine if we are currently "settling" - 
   // i.e: waiting for brokers to announce their presence so that 
   // we get a full view of the queue. if settling isn't
   // performed, then the queue can be only partially set up and 
   // jobs can go to the wrong broker. this isn't a massive 
   // problem, but can cause races later on.
   boost::optional<boost::posix_time::ptime> m_settle_time;

   // time to consider a broker alive (i.e: heartbeat must have been 
   // received in this past time)
   boost::posix_time::time_duration liveness_time;

   // the time to reconnect all the subscription sockets. this should
   // be fairly long - these effects don't seem to happen for a matter
   // of days.
   boost::posix_time::ptime m_next_sub_reconnect;
   boost::posix_time::time_duration m_sub_reconnect_interval;
  
   /* also, we need a timer and queue to check for timed-out requests and
    * resubmit them.
    */
   struct submit_info {
      boost::posix_time::ptime last_submission;
      size_t num_tries;
   };
   boost::unordered_multimap<job_t, submit_info> resubmit_queue;
  
   // need a structure to tell which brokers we received heartbeats from
   // recently, and how recently.
   struct heartbeat {
      heartbeat();
      boost::posix_time::ptime time; // when the last heartbeat was received
      uint64_t queue_size; // size of the broker's queue, as advertised.
      bool is_live; // whether the *consistent hash* considers this live.
   };
   boost::unordered_map<std::string, heartbeat> heartbeats;

   // the list of live brokers at the last count. so that the consistent hash
   // can be updated in stages, as the normal operating procedure should be
   // for them all to be up.
   std::list<std::string> live_brokers;

   // figure out which brokers are live, and which were live at the last check
   // and update the consistent hash function to match.
   void update_live_brokers();

   // update the heartbeat for a broker.
   void update_heartbeat(const std::string &broker_id, uint64_t qsize);

   // check if we are still settling. returns true if settling has not
   // yet finished, false if the queue is ready to be used.
   bool settle_check();
   bool settle_check() const;
};

/**
 * 0MQ-based backend for the queue, from the worker's point of view. The 
 * worker connection is handled by a different class, zmq_backend_worker.
 *
 * Note that this requires a running broker process.
 */
class zmq_backend_worker : public supervisor_backend {
public:
   // sets up the backend with its own context
   zmq_backend_worker(const boost::property_tree::ptree &pt);

   /* sets up the backend sharing a context. this is necessary for 0MQ to poll
    * properly when used in the worker with other 0MQ sockets.
    */
   zmq_backend_worker(const boost::property_tree::ptree &pt, zmq::context_t &context);

   ~zmq_backend_worker(); // no-throw

   job_t get_job();
   void notify(const job_t &job);

private:
   // whether the worker backend owns the zmq context.
   bool owns_context;

   // the context that the backend uses - possibly doesn't own.
   zmq::context_t *context;

   // the socket to communicate with the thread which handles the other
   // communication.
   zstream::socket::pair inproc_rep;

   // set to true when the destructor is called so that the communicator
   // thread can exit cleanly.
   bool shutdown_requested;

   // timeout for polls
   long poll_timeout;

   // unique ID for the worker which is applied to the zmq socket.
   std::string worker_id;

   struct task_communicator;
   boost::scoped_ptr<task_communicator> communicator;

   // thread which handles the communication
   boost::scoped_ptr<boost::thread> comm_thread;

   // common set up process shared between constructors
   void setup(const boost::property_tree::ptree &config);
};

}

#endif /* ZMQ_BACKEND_HPP */

