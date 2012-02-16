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

#ifndef PGQ_BACKEND_HPP
#define PGQ_BACKEND_HPP

#include <zmq.hpp>
#include <boost/property_tree/ptree.hpp>
#include "backend.hpp"

typedef struct pg_conn PGconn ;

namespace dqueue {
/**
 * PostgreSQL-based backend. Uses a table for storing jobs and async
 * notifications to inform the originator when a job has completed.
 * This is a common implementation, shared between the two
 * interfaces, since the PGQ implementation is more symmetric than
 * the 0MQ one.
 */
class pgq_backend 
{
public:
   pgq_backend(const boost::property_tree::ptree &pt);
   ~pgq_backend(); // no-throw

   void send(const job_t &job);

   job_t get_job();
   void notify(const job_t &job);

   int num_pollitems();
   void fill_pollitems(zmq::pollitem_t *items);
   bool handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs);

   size_t queue_length() const;

private:

   PGconn *conn;
};

/**
 * Runner / front-end view of the PGQ.
 */
class pgq_runner 
   : public runner_backend
{
public:
   pgq_runner(const boost::property_tree::ptree &pt);
   ~pgq_runner(); // no-throw

   void send(const job_t &job);

   int num_pollitems();
   void fill_pollitems(zmq::pollitem_t *items);
   bool handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs);

   size_t queue_length() const;

private:
   boost::shared_ptr<pgq_backend> impl;
};

/**
 * Supervisor / worker / back-end view of the PGQ.
 */
class pgq_supervisor
   : public supervisor_backend
{
public:
   pgq_supervisor(const boost::property_tree::ptree &pt);
   ~pgq_supervisor(); // no-throw

   job_t get_job();
   void notify(const job_t &job);

private:
   boost::shared_ptr<pgq_backend> impl;
};

}

#endif /* PGQ_BACKEND_HPP */

