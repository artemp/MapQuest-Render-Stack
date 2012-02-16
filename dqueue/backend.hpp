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

#ifndef BACKEND_HPP
#define BACKEND_HPP

#include <zmq.hpp>
#include <list>
#include <string>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/property_tree/ptree.hpp>
#include "distributed_queue.hpp"

namespace dqueue {
  
/**
 * Asynchronous tile rendering queue backend interface, from the 
 * job submission side.
 *
 * Implementations of this provide the logic to send and receive tile
 * requests from their respective queues. Although 0MQ is pulled in as
 * a requirement, it's not necessarily the technology of backend.
 *
 * TODO: The event loop multiplexing is a pretty ugly interface, and
 * only works if the event loop is using zmq::poll. This is currently
 * the only event loop we have, so it's not a pressing issue, but I 
 * can't help thinking there's A Better Way to do it.
 */
struct runner_backend 
   : public boost::noncopyable {
   // remember not to throw ;-)
   virtual ~runner_backend() {}

   /* Send a job to the queue. This can block until the job is in the
    * queue, but must not block for completion of the job.
    */
   virtual void send(const job_t &job) = 0;

   /* Ugly interface to allow the originator to integrate into a 0MQ
    * event loop. Currently, this is in the handler. The num_ and fill_
    * functions are to set up a pollitem array and handle_ is called
    * after the poll completes.
    *
    * The num_pollitems call should return the number of file descriptors
    * or zmq::socket_t entries that this backend is interested in activity
    * on. The fill_pollitems call will then be passed an array with that
    * many entries free to be set up by this code. When handle_pollitems
    * is called the items are in the same order, and the jobs list should
    * be filled with any notified jobs, returning true if there are jobs
    * complete.
    */
   virtual int num_pollitems() = 0;
   virtual void fill_pollitems(zmq::pollitem_t *items) = 0;
   virtual bool handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs) = 0;

   // return the (approximate) size of the rendering queue, normalised to
   // the number of running brokers.
   virtual size_t queue_length() const = 0;
};

/**
 * Asynchronous tile rendering queue interface, as seen from the worker's 
 * perspective.
 */
struct supervisor_backend 
   : public boost::noncopyable 
{
   // remember not to throw ;-)
   virtual ~supervisor_backend() {}
   
   /* Blocking call to retrieve a job from the queue.
    */
   virtual job_t get_job() = 0;

   /* Signals the originator of the job that it has been processed.
    * Note that the job passed in might have been modified from the job
    * which was originally queued. At the moment this should be 
    * confined to changes to the "status" field.
    */
   virtual void notify(const job_t &job) = 0;
};

typedef supervisor_backend *(*supervisor_creator)(const boost::property_tree::ptree &);
typedef runner_backend *(*runner_creator)(const boost::property_tree::ptree &, zmq::context_t &);

bool register_backend(const std::string &type, runner_creator runner, supervisor_creator supervisor);
runner_backend *create_runner(const boost::property_tree::ptree &, zmq::context_t &);
supervisor_backend *create_supervisor(const boost::property_tree::ptree &);

}

#endif /* BACKEND_HPP */

