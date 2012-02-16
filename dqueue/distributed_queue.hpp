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

#ifndef DISTRIBUTED_QUEUE_HPP
#define DISTRIBUTED_QUEUE_HPP

#include <zmq.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include <boost/unordered_map.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/optional.hpp>

#include "../tile_protocol.hpp"
#include <list>

namespace dqueue {

typedef rendermq::tile_protocol job_t;

struct runner_backend;
struct supervisor_backend;

/* gets thrown when there is an error sending data to the broker, either
 * because it's unavailable or because there was some other kind of 
 * error. currently it's used as a retry signal when there aren't any
 * ready brokers.
 */
class broker_error : public std::exception {
public:
   explicit broker_error(const std::string &reason);
   broker_error(const std::string &reason, const std::exception &cause);
   virtual ~broker_error() throw();
   virtual const char* what() const throw();
private:
   std::string what_;
};

class runner 
   : public boost::noncopyable {
public:
   typedef boost::function<void (const job_t &)> handler_function_t;
    
   runner(const std::string &config_file, zmq::context_t &context);
   ~runner();

   runner &default_handler(handler_function_t default_h);
    
   void put_job(const job_t &job);
   void put_job(const job_t &job, handler_function_t handler);

   // ugly bit of the interface for dealing with zmq poll. doesn't seem to be a way of encapsulating
   // this without taking control for the event loop away from the program calling this library.
   int num_pollitems();
   void fill_pollitems(zmq::pollitem_t *items);
   void handle_pollitems(zmq::pollitem_t *items);

   // find out what the queue length is. given the distributed nature of the queue, the
   // length at any point in time cannot be exactly guaranteed, but it should be enough
   // to get a rough idea of how busy the system is. 
   // NOTE: the queue length is (approximately) normalised by the number of active 
   // brokers this runner can see. this isn't necessarily the best normalisation that
   // could be done, but it seems to make more sense than the sum of all the queue
   // lengths.
   size_t queue_length() const;
    
private:
   boost::scoped_ptr<runner_backend> pimpl;
    
   handler_function_t default_message_handler;
   typedef boost::unordered_map<job_t, handler_function_t> handler_route_map_t;
   handler_route_map_t message_handlers;

   // utility function for dispatching jobs to message handlers.
   void handle_jobs(std::list<job_t> &jobs);
};

class supervisor 
   : public boost::noncopyable {
public:
   // separate constructor allows manual override of auto-generated worker ID, which
   // has benefits for traceability and failure recovery. NOTE: the defaulted worker
   // ID (empty string) is a *really* bad way of doing this, but the right way of 
   // doing it (boost::optional) doesn't play nice with boost::python.
   supervisor(const std::string &config_file, std::string worker_id = "");
    
   job_t get_job();
   void notify(const job_t &job);
    
private:
   boost::scoped_ptr<supervisor_backend> pimpl;
};

}

#endif /* DISTRIBUTED_QUEUE_HPP */
