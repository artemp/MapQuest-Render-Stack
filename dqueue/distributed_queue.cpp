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

#include "distributed_queue.hpp"
#include "../tile_protocol.hpp"
#include "../proto/tile.pb.h"
#include "zmq_backend.hpp"
#include "pgq_backend.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/scope_exit.hpp>
#include <boost/foreach.hpp>

#include <stdexcept>
#include <sstream>

namespace dqueue {

using std::ostringstream;
namespace pt = boost::property_tree;

namespace {
runner_backend *configure_backend(const std::string &config_file, zmq::context_t &context) {
   pt::ptree pt;
        
   try {
      pt::read_ini(config_file,pt);
   } catch (pt::ptree_error &err) {
      std::ostringstream ostr;
      ostr << "Unable to read config file `" << config_file << "': " << err.what();
      throw std::runtime_error(ostr.str());
   }

   try {
      std::string backend_type = pt.get<std::string>("backend.type");
      return create_runner(pt, context);

   } catch (pt::ptree_error &err) {
      std::ostringstream ostr;
      ostr << "Unknown or unspecified backend type in `" << config_file << "': " << err.what();
      throw std::runtime_error(ostr.str());
   }
}
}

broker_error::broker_error(const std::string &reason) 
   : what_(reason) {
}

broker_error::broker_error(const std::string &reason, const std::exception &cause) {
   ostringstream ostr;
   ostr << reason << " Caused by: " << cause.what();
   what_ = ostr.str();
}

broker_error::~broker_error() throw() {
}

const char *
broker_error::what() const throw() {
   return what_.c_str();
}

runner::runner(const std::string &config_file, zmq::context_t &context) 
   : pimpl(configure_backend(config_file, context)) {
}

runner::~runner() {
}

runner &
runner::default_handler(handler_function_t default_h) {
   default_message_handler = default_h;
   // check that the handler actually points to a usable function
   assert(bool(default_message_handler));
   return *this;
}

void
runner::put_job(const job_t &job) {
   pimpl->send(job);
}

void
runner::put_job(const job_t &job, handler_function_t handler) {
   message_handlers.insert(std::make_pair(job, handler));
   pimpl->send(job);
}

int
runner::num_pollitems() {
   return pimpl->num_pollitems();
}

void 
runner::fill_pollitems(zmq::pollitem_t *items) {
   pimpl->fill_pollitems(items);
}

void
runner::handle_pollitems(zmq::pollitem_t *items) {
   std::list<job_t> jobs;
   if (pimpl->handle_pollitems(items, jobs)) {
      handle_jobs(jobs);
   }
}

size_t 
runner::queue_length() const {
   return pimpl->queue_length();
}

void
runner::handle_jobs(std::list<job_t> &jobs) 
{
   BOOST_FOREACH(job_t job, jobs) {
      handler_route_map_t::iterator h_itr = message_handlers.find(job);
      if (h_itr != message_handlers.end()) {
         // call handler
         h_itr->second(job);
         
         BOOST_SCOPE_EXIT( (h_itr)(&message_handlers) ) {
            // handlers are (currently) one use only...
            message_handlers.erase(h_itr);
         } BOOST_SCOPE_EXIT_END;
         
      } else {
         // call the default handler - NOTE: error handling is taken care of by
         // boost::function, so there's no chance of disappearing off into 
         // uninitialised space.
         default_message_handler(job);
      }
   }
}

supervisor::supervisor(const std::string &config_file, std::string worker_id) {
   pt::ptree pt;

   try {
      pt::read_ini(config_file,pt);
   } catch (pt::ptree_error &err) {
      std::ostringstream ostr;
      ostr << "Unable to read config file `" << config_file << "': " << err.what();
      throw std::runtime_error(ostr.str());
   }

   // override the worker id, if it was provided
   if (!worker_id.empty()) {
      pt.put("worker.id", worker_id);
   }

   try {
      std::string backend_type = pt.get<std::string>("backend.type");
      pimpl.reset(create_supervisor(pt));

   } catch (pt::ptree_error &err) {
      std::ostringstream ostr;
      ostr << "Unknown or unspecified backend type in `" << config_file << "': " << err.what();
      throw std::runtime_error(ostr.str());
   }
}

job_t
supervisor::get_job() {
   return pimpl->get_job();
}

void
supervisor::notify(const job_t &job) {
   pimpl->notify(job);
}

}
