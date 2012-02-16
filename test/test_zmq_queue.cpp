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

#include "dqueue/zmq_backend.hpp"
#include "test/common.hpp"
#include "test/fake_tile.hpp"
#include "tile_broker_impl.hpp"
#include "dqueue/distributed_queue_config.hpp"
#include "logging/logger.hpp"

#include <stdexcept>
#include <iostream>
#include <boost/function.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/array.hpp>
#include <boost/format.hpp>
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/filesystem.hpp>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <cstdio>

using boost::function;
using boost::shared_ptr;
using std::runtime_error;
using std::exception;
using std::cout;
using std::endl;
using std::string;
using std::list;
using std::map;
using std::set;
using std::ostringstream;
namespace pt = boost::property_tree;
namespace fs = boost::filesystem;

using rendermq::cmdRender;
using rendermq::cmdRenderPrio;
using rendermq::cmdDone;
using rendermq::fmtPNG;
using rendermq::fmtJPEG;

namespace {

string broker_monitor(const pt::ptree &config, const string &broker_name) {
  dqueue::conf::common dconf(config);
  map<string,dqueue::conf::broker>::iterator self = dconf.brokers.find(broker_name);
  if (self == dconf.brokers.end()) {
    ostringstream ostr;
    ostr << "Broker name `" << broker_name << "' isn't present in config file.";
    throw runtime_error(ostr.str());
  }
  return self->second.monitor;
}

template <typename T>
string to_string(const T &t) {
  ostringstream ostr;
  ostr << t;
  return ostr.str();
}

struct test_base {
  virtual ~test_base();

  // override this to actually provide the test
  virtual void do_test(
    list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
    list<shared_ptr<dqueue::zmq_backend_worker> > &workers) = 0;

  void operator()();

  void poll_handler(
    dqueue::zmq_backend_handler &handler,
    list<rendermq::tile_protocol> &job_list);

  void setup_broker_configs(pt::ptree &config);

  list<string> broker_names;
  unsigned int num_workers;
  unsigned int num_handlers;
  // keep the command sockets here, so that they can be manipulated by 
  // the test procedure.
  list<shared_ptr<zstream::socket::req> > cmd_sockets;
   // location of temporary directory for sockets
   fs::path tmpdir;
};

test_base::~test_base() 
{
   if (fs::exists(tmpdir))
   {
      fs::remove_all(tmpdir);
   }
}

void 
test_base::setup_broker_configs(pt::ptree &config) {
  { // write comma separated list of all broker names
    ostringstream ostr;
    list<string>::iterator itr = broker_names.begin();
    ostr << *itr++;
    for (; itr != broker_names.end(); ++itr) {
      ostr << "," << *itr;
    }
    config.put("zmq.broker_names", ostr.str());
  }

  // create a temporary directory to put broker sockets in
  // so that we don't pollute global TCP port space. otherwise
  // running multiple tests can clash with eachother, giving
  // a false failure.
  fs::path tmpdir = fs::path("/tmp") / fs::unique_path();
  if (!fs::create_directories(tmpdir)) 
  {
     throw runtime_error("Cannot create temporary directory for broker sockets");
  }

  // write each broker section
  BOOST_FOREACH(string broker_name, broker_names) 
  {
     config.put(broker_name + ".in_req", "ipc://" + tmpdir.string() + broker_name + ".in_req");
     config.put(broker_name + ".in_sub", "ipc://" + tmpdir.string() + broker_name + ".in_sub");
     config.put(broker_name + ".out_req", "ipc://" + tmpdir.string() + broker_name + ".out_req");
     config.put(broker_name + ".out_sub", "ipc://" + tmpdir.string() + broker_name + ".out_sub");
     config.put(broker_name + ".monitor", "ipc://" + tmpdir.string() + broker_name + ".monitor");
  }
}

void
test_base::operator()() {
  // set up a fake config with ipc:// sockets in /tmp
  pt::ptree config;
  unsigned int heartbeat_time = 1;
  string broker_name("broker_name");
  
  config.put("backend.type", "zmq");
  config.put("zmq.heartbeat_time", heartbeat_time);
  config.put("zmq.zombie_time", 5);
  config.put("zmq.liveness_time", 3 * heartbeat_time);
  config.put("worker.poll_timeout", "1");
  setup_broker_configs(config);

  try {
    zmq::context_t ctx(1);
    
    // setup a thread for the broker
    list<shared_ptr<rendermq::broker_impl> > brokers;
    list<shared_ptr<boost::thread> > broker_threads;
    
    BOOST_FOREACH(string broker_name, broker_names) {
      shared_ptr<rendermq::broker_impl> broker(new rendermq::broker_impl(config, broker_name, ctx));
      shared_ptr<boost::thread> broker_thread(new boost::thread(boost::ref(*broker)));
      
      // connect to the broker's monitor socket
      shared_ptr<zstream::socket::req> cmd(new zstream::socket::req(ctx));
      cmd->connect(broker_monitor(config, broker_name));
      
      brokers.push_back(broker);
      broker_threads.push_back(broker_thread);
      cmd_sockets.push_back(cmd);
    }
    
    // internal section to ensure object destruction in correct order
    {
      // start up some handlers...
      list<shared_ptr<dqueue::zmq_backend_handler> > handlers;
      for (unsigned int i = 0; i < num_handlers; ++i) {
        handlers.push_back(
          shared_ptr<dqueue::zmq_backend_handler>(
            new dqueue::zmq_backend_handler(config, ctx)));
      }

      // start up some workers...
      list<shared_ptr<dqueue::zmq_backend_worker> > workers;
      for (unsigned int i = 0; i < num_workers; ++i) {
        workers.push_back(
          shared_ptr<dqueue::zmq_backend_worker>(
            new dqueue::zmq_backend_worker(config, ctx)));
      }
      
      // spend a little time in the event loop to allow the broker and 
      // handler to hook up.
      for (unsigned int count = 0; count < 5 * heartbeat_time; ++count) {
        list<rendermq::tile_protocol> dummy_list;
        BOOST_FOREACH(shared_ptr<dqueue::zmq_backend_handler> handler, handlers) {
          poll_handler(*handler, dummy_list);
          if (dummy_list.size() != 0) {
            throw runtime_error("handler got an unexpected job whilst in the idle setup loop.");
          }
        }
      }

      try {
        do_test(handlers, workers);
      } catch (const exception &e) {
         LOG_ERROR(boost::format("CAUGHT: %1%") % e.what());
         throw;
      }
    }
    
    // send shutdown signal to broker thread
    BOOST_FOREACH(shared_ptr<zstream::socket::req> cmd, cmd_sockets) {
      string shutdown_command("SHUTDOWN");
      (*cmd) << shutdown_command;
      (*cmd) >> shutdown_command;
    }
    // get rid of the sockets, else when the context is terminated
    // it will hang...
    cmd_sockets.clear();
    
    // catch up to the broker thread
    BOOST_FOREACH(shared_ptr<boost::thread> thread, broker_threads) {
      thread->join();
    }

  } catch (const exception &e) {
     LOG_ERROR(boost::format("CAUGHT: %1%") % e.what());
     throw;
  }
}

void 
test_base::poll_handler(
  dqueue::zmq_backend_handler &handler, 
  list<rendermq::tile_protocol> &rv_job_list) {

  zmq::pollitem_t items[] = {
    { NULL, 0, ZMQ_POLLIN, 0 },
    { NULL, 0, ZMQ_POLLIN, 0 }
  };
  handler.fill_pollitems(items);
  zmq::poll(items, 2, 300000);
  handler.handle_pollitems(items, rv_job_list);
}

struct test_end_to_end_single
  : public test_base {
  test_end_to_end_single() {
    broker_names.push_back("broker1");
    num_workers = 1;
    num_handlers = 1;
  }
  virtual ~test_end_to_end_single() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();
    dqueue::zmq_backend_handler &handler = **handlers.begin();

    // send a job
    rendermq::tile_protocol job_put(cmdRender, 5, 3, 4, 101010, "foo", fmtPNG, 0, std::time(0));
    handler.send(job_put);
    
    // get a job at the worker
    rendermq::tile_protocol job_get = worker.get_job();
    
    // check that job recieved is the same as the one sent, but expanded to a metatile
    if (job_get.x != (job_put.x & ~0x7)) {
      throw runtime_error("job received has different X metatile than the one sent by handler.");
    }
    if (job_get.y != (job_put.y & ~0x7)) {
      throw runtime_error("job received has different Y metatile than the one sent by handler.");
    }
    if (job_get.z != job_put.z) {
      throw runtime_error("job received has different Z than the one sent by handler.");
    }
    if (job_get.style != job_put.style) {
      throw runtime_error("job received has different style than the one sent by handler.");
    }
    if (job_get.status != job_put.status) {
      throw runtime_error("job received has different status than the one sent by handler.");
    }
    if (job_get.id != job_put.id) {
      throw runtime_error("job received has different client ID than the one sent by handler.");
    }
    if (job_get.last_modified != job_put.last_modified) {
      throw runtime_error("job received has different last modified time than the one sent by handler.");
    }
    if (job_get.request_last_modified != job_put.request_last_modified) {
      throw runtime_error("job received has different last modified request time than the one sent by handler.");
    }
    
    // send a result back
    job_get.status = cmdDone;
    job_get.last_modified = job_get.request_last_modified + 600;
    worker.notify(job_get);

    list<rendermq::tile_protocol> rv_job_list;
    poll_handler(handler, rv_job_list);
    if (rv_job_list.size() != 1) {
      throw runtime_error("didn't get tile response from worker.");
    }
    if (!(rv_job_list.front() == job_put)) {
      throw runtime_error("job received at handler isn't the same as the one returned from broker.");
    }
    if (rv_job_list.front().status != cmdDone) {
      throw runtime_error("job received at handler doesn't have status as set by the worker.");
    }
    if (rv_job_list.front().last_modified != job_get.request_last_modified + 600) {
      throw runtime_error("job received at handler doesn't have last modified as set by the worker.");
    }
  }
};

struct test_end_to_end_single_priority
  : public test_base {
  test_end_to_end_single_priority() {
    broker_names.push_back("broker1");
    num_workers = 1;
    num_handlers = 1;
  }
  virtual ~test_end_to_end_single_priority() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();
    dqueue::zmq_backend_handler &handler = **handlers.begin();

    rendermq::tile_protocol jobs[4];
    jobs[0] = rendermq::tile_protocol(cmdRender,      0,  0, 4, 101010, "foo", fmtPNG);
    jobs[1] = rendermq::tile_protocol(cmdRender,      8,  8, 5, 101010, "foo", fmtPNG);
    jobs[2] = rendermq::tile_protocol(cmdRenderPrio, 16, 16, 5, 202020, "bar", fmtPNG);
    jobs[3] = rendermq::tile_protocol(cmdRender,      0,  0, 6, 101010, "foo", fmtPNG);
    
    // send several jobs
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      handler.send(jobs[i]);
    }

    // wait for broker to catch up and process the jobs.
    usleep(10000);

    {
      // get a job at the worker - this should be the highest priority one.
      rendermq::tile_protocol job_get = worker.get_job();
      if (!(job_get == jobs[2])) {
        ostringstream ostr;
        ostr << "first job received by worker wasn't the highest priority one. "
             << "expected: " << jobs[2] << ", "
             << "actual: " << job_get << ".";
        throw runtime_error(ostr.str());
      }
    
      // send a result back
      job_get.status = cmdDone;
      worker.notify(job_get);
    }

    // drain the other jobs
    for (size_t i = 1; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      rendermq::tile_protocol job_get = worker.get_job();
    
      job_get.status = cmdDone;
      worker.notify(job_get);
    }      

    size_t count = 0;
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      list<rendermq::tile_protocol> rv_job_list;
      poll_handler(handler, rv_job_list);
      count += rv_job_list.size();
    }
    if (count != 4) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

struct test_end_to_end_multiple
  : public test_base {
  test_end_to_end_multiple() {
    broker_names.push_back("broker1");
    broker_names.push_back("broker2");
    broker_names.push_back("broker3");
    num_workers = 1;
    num_handlers = 1;
  }
  virtual ~test_end_to_end_multiple() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();
    dqueue::zmq_backend_handler &handler = **handlers.begin();
    rendermq::tile_protocol jobs[4];

    jobs[0] = rendermq::tile_protocol(cmdRender,      0,  0, 4, 101010, "foo", fmtPNG);
    jobs[1] = rendermq::tile_protocol(cmdRender,      8,  8, 5, 101010, "foo", fmtPNG);
    jobs[2] = rendermq::tile_protocol(cmdRenderPrio, 16, 16, 5, 202020, "bar", fmtPNG);
    jobs[3] = rendermq::tile_protocol(cmdRender,      0,  0, 6, 101010, "foo", fmtPNG);
    
    // send several jobs
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      handler.send(jobs[i]);
    }
    
    // drain the other jobs
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      rendermq::tile_protocol job_get = worker.get_job();
    
      job_get.status = cmdDone;
      worker.notify(job_get);
    }

    size_t count = 0;
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      list<rendermq::tile_protocol> rv_job_list;
      poll_handler(handler, rv_job_list);
      count += rv_job_list.size();
      usleep(10000);
    }
    if (count != 4) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

struct test_worker_failure
  : public test_base {
  test_worker_failure() {
    broker_names.push_back("broker1");
    num_workers = 2;
    num_handlers = 1;
  }
  virtual ~test_worker_failure() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &first_worker = **workers.begin();
    dqueue::zmq_backend_worker &second_worker = **(++workers.begin());
    dqueue::zmq_backend_handler &handler = **handlers.begin();

    rendermq::tile_protocol jobs[4];
    jobs[0] = rendermq::tile_protocol(cmdRender,      0,  0, 4, 101010, "foo", fmtPNG);
    jobs[1] = rendermq::tile_protocol(cmdRender,      8,  8, 5, 101010, "foo", fmtPNG);
    jobs[2] = rendermq::tile_protocol(cmdRenderPrio, 16, 16, 5, 202020, "bar", fmtPNG);
    jobs[3] = rendermq::tile_protocol(cmdRender,      0,  0, 6, 101010, "foo", fmtPNG);
    
    // send several jobs
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      handler.send(jobs[i]);
    }
    
    // get a job at the worker and swallow it, as if the worker had 
    // failed.
    first_worker.get_job();

    // drain the jobs, expecting that the broker will resumbit the
    // job after some timeout.
    for (size_t i = 0; i < sizeof(jobs)/sizeof(rendermq::tile_protocol); ++i) {
      rendermq::tile_protocol job_get = second_worker.get_job();
    
      job_get.status = cmdDone;
      second_worker.notify(job_get);
    }

    // let worker events percolate a little
    usleep(100000);

    size_t count = 0;
    for (size_t i = 0; i < 10; ++i) {
      list<rendermq::tile_protocol> rv_job_list;
      poll_handler(handler, rv_job_list);
      LOG_DEBUG(boost::format("Handler got jobs size = %1%") % rv_job_list.size());
      count += rv_job_list.size();
    }
    if (count != 4) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

/* checks that the number of jobs executed by the workers is correct - it
 * should be the number of distinct metatile requests issued by the handler,
 * which is less than or equal to the actual number of requests.
 */
struct test_multiple_broker_collapsing
  : public test_base {
  test_multiple_broker_collapsing() {
    broker_names.push_back("broker1");
    broker_names.push_back("broker2");
    broker_names.push_back("broker3");
    num_workers = 1;
    num_handlers = 1;
  }
  virtual ~test_multiple_broker_collapsing() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();
    dqueue::zmq_backend_handler &handler = **handlers.begin();
    const size_t num_jobs = 100;
    size_t num_unique_jobs = 0;
    size_t count = 0;

    { // count the number that end up as the same metatile...
      set<size_t> job_hashes;
      // send several jobs
      for (size_t i = 0; i < num_jobs; ++i) {
        int z = int(ceil(log2(i+1)));
        rendermq::tile_protocol job(cmdRenderPrio, i, i, z, i, "", fmtPNG);
        if (z > 18 || i > ((1u << z) - 1)) { throw runtime_error("Unexpectedly made a bad tile request."); }
        handler.send(job);
        job_hashes.insert(rendermq::hash_value(job));
      }
      num_unique_jobs = job_hashes.size();
    }

    // let broker events percolate a little
    usleep(100000);

    for (size_t i = 0; i < num_unique_jobs; ++i) {
      rendermq::tile_protocol job_get = worker.get_job();
    
      job_get.status = cmdDone;
      worker.notify(job_get);
    }

    // let worker events percolate a little
    usleep(100000);

    for (size_t i = 0; i < (num_jobs + 10); ++i) {
      list<rendermq::tile_protocol> rv_job_list;
      poll_handler(handler, rv_job_list);
      LOG_DEBUG(boost::format("Handler got jobs size = %1%") % rv_job_list.size());
      count += rv_job_list.size();
    }
    if (count != num_jobs) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

/* checks that the system works correctly in the face of broker failure.
 */
struct test_broker_failure
  : public test_base {
  test_broker_failure() {
    broker_names.push_back("broker1");
    broker_names.push_back("broker2");
    broker_names.push_back("broker3");
    num_workers = 1;
    num_handlers = 1;
  }
  virtual ~test_broker_failure() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();
    dqueue::zmq_backend_handler &handler = **handlers.begin();
    const size_t num_jobs = 100;
    size_t num_unique_jobs = 0;
    size_t count = 0;

    { // broker(s) have already hooked up with the handlers, so kill
      // one and see what happens.
      list<shared_ptr<zstream::socket::req> >::iterator cmd_itr = cmd_sockets.begin();
      shared_ptr<zstream::socket::req> cmd = *cmd_itr;
      // remove the socket so the broker isn't killed twice.
      cmd_sockets.erase(cmd_itr);
      // kill the broker.
      string command("SHUTDOWN");
      (*cmd) << command;
      (*cmd) >> command;
      if (command != "SHUTDOWN") { throw runtime_error("Couldn't kill a broker."); }

      for (int i = 0; i < 100; ++i) {
        list<rendermq::tile_protocol> rv_job_list;
        poll_handler(handler, rv_job_list);
        usleep(10000);
        //std::cerr << "Ping" << std::endl << std::endl << std::endl;
      }
    }

    { // count the number that end up as the same metatile...
      set<size_t> job_hashes;
      // send several jobs
      for (size_t i = 0; i < num_jobs; ++i) {
        int z = int(ceil(log2(i+1)));
        rendermq::tile_protocol job(cmdRenderPrio, i, i, z, i, "", fmtPNG);
        if (z > 18 || i > ((1u << z) - 1)) { throw runtime_error("Unexpectedly made a bad tile request."); }
        handler.send(job);
        job_hashes.insert(rendermq::hash_value(job));
      }
      num_unique_jobs = job_hashes.size();
    }

    // let broker events percolate a little
    usleep(100000);

    for (size_t i = 0; i < num_unique_jobs; ++i) {
      rendermq::tile_protocol job_get = worker.get_job();
    
      job_get.status = cmdDone;
      worker.notify(job_get);
    }

    // let worker events percolate a little
    usleep(100000);

    for (size_t i = 0; i < (num_jobs + 10); ++i) {
      list<rendermq::tile_protocol> rv_job_list;
      poll_handler(handler, rv_job_list);
      LOG_DEBUG(boost::format("Handler got jobs size = %1%") % rv_job_list.size());
      count += rv_job_list.size();
    }
    if (count != num_jobs) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

struct test_multi_handler
  : public test_base {
  test_multi_handler() {
    broker_names.push_back("broker1");
    num_workers = 1;
    num_handlers = 2;
  }
  virtual ~test_multi_handler() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();

    dqueue::zmq_backend_handler &handler1 = **handlers.begin();
    dqueue::zmq_backend_handler &handler2 = **(++handlers.begin());

    // send a whole metatile
    // http://mq-openmapnik-lm01.ihost.aol.com:8000/tiles/1.0.0/osm/15/16357/10887.jpg
    size_t num_jobs = 0;
    for (int y = 10880; y < 10888; ++y) {
      for (int x = 16352; x < 16360; ++x) {
        rendermq::tile_protocol job(cmdRender, x, y, 15, x * 100000 + y, "foo", fmtJPEG);
        if ((x ^ y) & 1) {
          handler1.send(job);
        } else {
          handler2.send(job);
        }
        ++num_jobs;
      }
    }

    // get a job at the worker
    rendermq::tile_protocol job_get = worker.get_job();

    // check it's the right job.
    if (job_get.x != 16352) { throw runtime_error("Got job with different x than expected."); }
    if (job_get.y != 10880) { throw runtime_error("Got job with different y than expected."); }
    if (job_get.z != 15)    { throw runtime_error("Got job with different z than expected."); }

    // return the job
    usleep(100000);
    job_get.status = cmdDone;
    worker.notify(job_get);
    
    // let worker events percolate a little
    usleep(100000);

    size_t count = 0;
    for (size_t i = 0; i < (num_jobs + 5); ++i) {
      BOOST_FOREACH(shared_ptr<dqueue::zmq_backend_handler> handler, handlers) {
        list<rendermq::tile_protocol> rv_job_list;
        poll_handler(*handler, rv_job_list);
        LOG_DEBUG(boost::format("Handler got jobs size = %1%, total = %2%") % rv_job_list.size() % count);
        count += rv_job_list.size();
      }
    }
    if (count != num_jobs) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

struct test_multi_handler_interleaved
  : public test_base {
  test_multi_handler_interleaved() {
    broker_names.push_back("broker1");
    num_workers = 1;
    num_handlers = 2;
  }
  virtual ~test_multi_handler_interleaved() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();

    dqueue::zmq_backend_handler &handler1 = **handlers.begin();
    dqueue::zmq_backend_handler &handler2 = **(++handlers.begin());

    // send a whole metatile
    size_t num_jobs = 0;
    { // send one job, then follow up with others...
      int x = 16352; int y = 10880;
      rendermq::tile_protocol job(cmdRender, x, y, 15, x * 100000 + y, "foo", fmtJPEG);
      handler1.send(job);
      ++num_jobs;
    }

    // get a job at the worker
    rendermq::tile_protocol job_get = worker.get_job();

    // check it's the right job.
    if (job_get.x != 16352) { throw runtime_error("Got job with different x than expected."); }
    if (job_get.y != 10880) { throw runtime_error("Got job with different y than expected."); }
    if (job_get.z != 15)    { throw runtime_error("Got job with different z than expected."); }

    // now send the rest of the tiles, which should just be appended to the
    // existing (in progress) job.
    for (int y = 10880; y < 10888; ++y) {
      for (int x = 16352; x < 16360; ++x) {
        rendermq::tile_protocol job(cmdRender, x, y, 15, x * 100000 + y, "foo", 
                                    (x + y) & 1 ? fmtJPEG : fmtPNG);
        if ((x ^ y) & 1) {
          handler1.send(job);
        } else {
          handler2.send(job);
        }
        ++num_jobs;
      }
    }

    // return the job
    usleep(100000);
    job_get.status = cmdDone;
    worker.notify(job_get);
    
    // let worker events percolate a little
    usleep(100000);

    size_t count = 0;
    for (size_t i = 0; i < (num_jobs + 5); ++i) {
      BOOST_FOREACH(shared_ptr<dqueue::zmq_backend_handler> handler, handlers) {
        list<rendermq::tile_protocol> rv_job_list;
        poll_handler(*handler, rv_job_list);
        LOG_DEBUG(boost::format("Handler got jobs size = %1%, total = %2%") % rv_job_list.size() % count);
        count += rv_job_list.size();
      }
    }
    if (count != num_jobs) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};

struct test_multi_handler_data
  : public test_base {
  test_multi_handler_data() {
    broker_names.push_back("broker1");
    broker_names.push_back("broker2");
    num_workers = 1;
    num_handlers = 2;
  }
  virtual ~test_multi_handler_data() {}

  void do_test(list<shared_ptr<dqueue::zmq_backend_handler> > &handlers,
               list<shared_ptr<dqueue::zmq_backend_worker> > &workers) {
    dqueue::zmq_backend_worker &worker = **workers.begin();

    dqueue::zmq_backend_handler &handler1 = **handlers.begin();
    dqueue::zmq_backend_handler &handler2 = **(++handlers.begin());

    // send a whole metatile
    size_t num_jobs = 0;
    { // send one job, then follow up with others...
      int x = 16352; int y = 10880;
      rendermq::tile_protocol job(cmdRender, x, y, 15, x * 100000 + y, "foo", fmtJPEG);
      handler1.send(job);
      ++num_jobs;
    }

    // get a job at the worker
    rendermq::tile_protocol job_get = worker.get_job();

    // check it's the right job.
    if (job_get.x != 16352) { throw runtime_error("Got job with different x than expected."); }
    if (job_get.y != 10880) { throw runtime_error("Got job with different y than expected."); }
    if (job_get.z != 15)    { throw runtime_error("Got job with different z than expected."); }

    // now send the rest of the tiles, which should just be appended to the
    // existing (in progress) job.
    for (int y = 10880; y < 10888; ++y) {
      for (int x = 16352; x < 16360; ++x) {
        rendermq::tile_protocol job(cmdRender, x, y, 15, x * 100000 + y, "foo", 
                                    (x + y) & 1 ? fmtJPEG : fmtPNG);
        if ((x ^ y) & 1) {
          handler1.send(job);
        } else {
          handler2.send(job);
        }
        ++num_jobs;
      }
    }

    // return the job
    usleep(100000);
    fake_tile meta(job_get.x, job_get.y, job_get.z, fmtPNG | fmtJPEG);
    job_get.set_data(string(meta.ptr, meta.total_size));
    job_get.status = cmdDone;
    worker.notify(job_get);
    
    // let worker events percolate a little
    usleep(100000);

    size_t count = 0;
    for (size_t i = 0; i < (num_jobs + 5); ++i) {
      BOOST_FOREACH(shared_ptr<dqueue::zmq_backend_handler> handler, handlers) {
        list<rendermq::tile_protocol> rv_job_list;
        poll_handler(*handler, rv_job_list);
        LOG_DEBUG(boost::format("Handler got jobs size = %1%, total = %2%") % rv_job_list.size() % count);
        count += rv_job_list.size();
        BOOST_FOREACH(rendermq::tile_protocol tile, rv_job_list) {
          char str[17];
          snprintf(str, 17, "%03d|%06d|%06d", tile.z, tile.x, tile.y);
          if (tile.data() != string(str)) {
            throw runtime_error(
              (boost::format("Tile data `%1%' != expected `%2%'")
               % tile.data() % str).str());
          }
        }
      }
    }
    if (count != num_jobs) {
      throw runtime_error("didn't get all tile responses from worker.");
    }
  }
};
} // anonymous namespace

int main() {
  int tests_failed = 0;

  cout << "== Testing 0MQ Distributed Queue ==" << endl << endl;

  tests_failed += test::run("test_end_to_end_single", test_end_to_end_single());
  tests_failed += test::run("test_end_to_end_single_priority", test_end_to_end_single_priority());
  tests_failed += test::run("test_end_to_end_multiple", test_end_to_end_multiple());
  tests_failed += test::run("test_worker_failure", test_worker_failure());
  tests_failed += test::run("test_multiple_broker_collapsing", test_multiple_broker_collapsing());
  tests_failed += test::run("test_broker_failure", test_broker_failure());
  tests_failed += test::run("test_multi_handler", test_multi_handler());
  tests_failed += test::run("test_multi_handler_interleaved", test_multi_handler_interleaved());
  tests_failed += test::run("test_multi_handler_data", test_multi_handler_data());
  //tests_failed += test::run("test_", &test_);

  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return (tests_failed > 0) ? 1 : 0;
}
