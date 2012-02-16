/*------------------------------------------------------------------------------
 *
 *  Asynchronous tile renderering queue backend interface.
 *
 *  Author: matt.amos@mapquest.com
 *  Author: david.lundeen@mapquest.com
 *  Author: john.novak@mapquest.com
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

#include "pgq_backend.hpp"

#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

#include <unistd.h>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/split.hpp>

#include <libpq-fe.h>

using std::vector;
using std::map;
using std::string;
using std::ostringstream;
using std::runtime_error;
using std::remove_copy_if;
using std::back_inserter;
using std::isalnum;
using std::locale;
using boost::format;
using boost::str;
using boost::lexical_cast;
using boost::bind;
using boost::find_all;
namespace pt = boost::property_tree;

/* There's some utility stuff in here to deal with queries and wrap
 * the basic C interface in some more C++-ish patterns. You may ask,
 * "why not use libpqxx, it's awesome". It is, but it doesn't yet
 * support many of the async calls that are important to this code.
 * Instead we have this minimal and probably buggy wrapper.
 */
namespace {
dqueue::runner_backend *create_pgq_runner(const pt::ptree &pt, zmq::context_t &) 
{
   return new dqueue::pgq_runner(pt);
}

dqueue::supervisor_backend *create_pgq_supervisor(const pt::ptree &pt)
{
   return new dqueue::pgq_supervisor(pt);
}

// register the PGQ backend
const bool registered = register_backend("pgq", create_pgq_runner, create_pgq_supervisor);

// very basic utility wrapper - checks that PGresults from queries
// aren't null. caller is responsible for freeing returned ptr.
PGresult *lowlevel_exec(PGconn *conn, const string &command) {
   PGresult *res = PQexec(conn, command.c_str());
        
   if (res == NULL) {
      // serious error - wasn't even able to allocate the result struct
      throw runtime_error("Fatal error: NULL result from PQexec.");
   }

   return res;
}

// utility wrapper to have some error handling around postgres calls
// which aren't expected to return any tuples.
void exec(PGconn *conn, const string &command) {
   PGresult *res = lowlevel_exec(conn, command);
   ExecStatusType status = PQresultStatus(res);
   PQclear(res);

   if (status != PGRES_COMMAND_OK) {
      // less serious error, but still pretty serious
      ostringstream ostr;
      ostr << "Error exec()ing postgres command: " << PQerrorMessage(conn);
      throw runtime_error(ostr.str());
   }
}

typedef vector<map<string, string> > result_t;

// utility wrapper around calls which we expect to return some tuples.
// copies the tuples so that the query can be deallocated - means this
// isn't a good function to call when expecting a lot of data.
result_t query(PGconn *conn, const string &command) {
   PGresult *res = lowlevel_exec(conn, command);

   result_t result;
   try {
      if (PQresultStatus(res) != PGRES_TUPLES_OK) {
         ostringstream ostr;
         ostr << "Error exec()ing postgres query: " << PQerrorMessage(conn);
         throw runtime_error(ostr.str());
      }

      const int ntuples = PQntuples(res);
      const int nfields = PQnfields(res);

      result.reserve(ntuples);
      for (int i = 0; i < ntuples; ++i) {
         map<string, string> row;
         for (int j = 0; j < nfields; ++j) {
            string field_name(PQfname(res, j));
            string value(PQgetvalue(res, i, j));
            row.insert(make_pair(field_name, value));
         }
         result.push_back(row);
      }
                
      PQclear(res);

   } catch (...) {
      PQclear(res);
      throw;
   }

   return result;
}

// RAII wrapper for transactions. ensures that they're either
// committed properly, rolled back on exception or the connection
// is FUBARed and it doesn't matter anyway.
struct transaction : public boost::noncopyable {
   transaction(PGconn *c, const string &n) 
      : conn(c), name(n), committed(false) {
      exec(conn, "begin transaction");
   }

   ~transaction() /* no-throw */ {
      if (!committed) {
         exec(conn, "rollback");
      }
   }

   void commit() {
      if (!committed) {
         exec(conn, "commit");
         committed = true;
      } else {
         throw runtime_error("Attempt to commit a transaction twice");
      }
   }

   string escape(const string &s) {
      char *esc = PQescapeLiteral(conn, s.c_str(), s.length());
      if (esc == 0) {
         throw runtime_error("Failed to escape string.");
      }
      string rv(esc);
      PQfreemem(esc);
      return rv;
   }

   PGconn * const conn;
   const string name;
   bool committed;
};

string safe_identifier(const string &s) {
   string t;
   locale loc;
   remove_copy_if(s.begin(), s.end(), back_inserter(t), !bind(&isalnum<char>, _1, loc));
   return t;
}

void parse_gid_and_style(const string &s, int64_t &gid, string &style) {
   vector<boost::iterator_range<string::const_iterator> > found;
   find_all(found, s, "_");
   if (found.size() >= 2) {
      gid = boost::lexical_cast<int64_t>(string(found[0].end(), found[1].begin()));
      style = string(found[1].end(), s.end());
   } else {
      ostringstream ostr;
      ostr << "Cannot parse GID & style from `" << s << "'.";
      throw runtime_error(ostr.str());
   } 
}

} // anonymous namespace

namespace dqueue {
pgq_backend::pgq_backend(const pt::ptree &pt) 
   : conn(PQconnectdb(pt.get<string>("pgq.broker").c_str())) {

   if ((conn == NULL) || (PQstatus(conn) != CONNECTION_OK)) {
      ostringstream ostr;
      ostr << "Cannot connect to PGQ broker: " << PQerrorMessage(conn);
      if (conn != NULL) PQfinish(conn);
      throw runtime_error(ostr.str());
   }

   // need serializable transaction level according to the Python code.
   exec(conn, "set transaction isolation level serializable");
}

pgq_backend::~pgq_backend() /* no-throw */ {
   PQfinish(conn);
}

void 
pgq_backend::send(const job_t &job) {
   transaction tx(conn, "send");

   // try to form the GID in the same way that the Python code does. note that this has
   // been expanded to include the "status" field also.
   int64_t gid = (int64_t(job.z) << 40) | (int64_t(job.x) << 20) | int64_t(job.y);
   string escaped_style = tx.escape(job.style);
   try {
      exec(conn, str(format("insert into tasks (gid, clientid, x, y, z, priority, status, style) values (%1%, %2%, %3%, %4%, %5%, %6%, %7%, E'%8%')") 
                     % gid % job.id % job.x % job.y % job.z % 1 % int(job.status) % escaped_style));
   } catch (...) {
      // presumably the case if the insert failed due to primary key constraints?
      result_t res = query(conn, str(format("insert into tasks (clientid, x, y, z, priority, status, style) values (%1%, %2%, %3%, %4%, %5%, %6%, E'%7%') returning gid") 
                                     % job.id % job.x % job.y % job.z % 1 % int(job.status) % escaped_style));
      gid = lexical_cast<int64_t>(res[0]["gid"]);
   }
   // listen to a channel named by the client ID. this allows us to get asynchronous responses from the workers
   // when they've finished processing a job via Postgres' notify command.
   exec(conn, str(format("listen client_%1%_%2%") % gid % safe_identifier(job.style)));

   tx.commit();
}

job_t 
pgq_backend::get_job() {
   int task_timeout = 30; // TODO: make configurable
   job_t job;
   bool waiting_for_job = true;

   do {
      transaction tx(conn, "get_job");
        
      result_t res =
         query(conn, str(format("select * from tasks where (scheduled is null or extract( epoch from (now() - scheduled ) )  > %1%) "
                                "and completed is null order by priority desc limit 1 for update") % task_timeout));

      if (res.size() == 1) {
         map<string, string> &row = res[0];

         int64_t gid = lexical_cast<int64_t>(row["gid"]);
         string style = row["style"];
         string escaped_style = tx.escape(style);

         exec(conn, str(format("update tasks set scheduled = now() where gid = %1% and style = %2%") % gid % escaped_style));

         job.status = (rendermq::protoCmd)lexical_cast<int>(row["status"]);
         job.x = lexical_cast<int>(row["x"]);
         job.y = lexical_cast<int>(row["y"]);
         job.z = lexical_cast<int>(row["z"]);
         job.id = gid;
         job.style = style;

         waiting_for_job = false;
         tx.commit();
      } else {
         // sleep? ugh... but without notifications going the other way (which is going to
         // complicate things even further). TODO: make this configurable.
         sleep(2);
      }
   } while (waiting_for_job);

   return job;
}

void 
pgq_backend::notify(const job_t &job) {
   transaction tx(conn, "notify");

   // get gid from job?
   int64_t gid = job.id;
   string escaped_style = tx.escape(job.style);

   // update the tasks table to show this task as done
   exec(conn, str(format("update tasks set completed = now(), status = %1% where gid = %2% and style = %3%") % job.status % gid % escaped_style));

   // send a notify through to the channel named by client ID.
   string foo(str(format("notify client_%1%_%2%") % gid % safe_identifier(job.style)));
   std::cerr << "foo = `" << foo << "', job style = " << job.style << std::endl;
   exec(conn, foo);

   tx.commit();
}

int 
pgq_backend::num_pollitems() {
   return 1;
}

void 
pgq_backend::fill_pollitems(zmq::pollitem_t *items) {
   items[0].socket = 0;
   items[0].fd = PQsocket(conn);
   items[0].events = ZMQ_POLLIN;
}

bool 
pgq_backend::handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs) {
   if (items[0].revents & ZMQ_POLLIN) {
      if (PQconsumeInput(conn) != 1) {
         ostringstream ostr;
         ostr << "Error while consuming input from socket: " << PQerrorMessage(conn);
         throw runtime_error(ostr.str());
      }

      // drain each notification - the relname should be "client_$GID" where the GID
      // is the primary key on the table.
      PGnotify *n = PQnotifies(conn);
      while (n != NULL) {
         transaction tx(conn, "read_one_result");
         // parse the GID (TODO: parse it properly, with some sort of error handling)
         int64_t gid = 0;
         string style;
         parse_gid_and_style(n->relname, gid, style);

         // this isn't going to work if the "safe" style name is different from the 
         // original style name... TODO: make sure all style names are safe as 
         // identifiers?
         string escaped_style = tx.escape(style);

         // query for *one* result from the tasks table
         result_t res = query(conn, str(format("select * from tasks where gid = %1% and style = %2%") % gid % escaped_style));

         // it seems we can get multiple notifies for the same GID...
         if (res.size() > 0) {
            map<string, string> &row = res[0];
            job_t job;

            job.status = (rendermq::protoCmd)lexical_cast<int>(row["status"]);
            job.x = lexical_cast<int>(row["x"]);
            job.y = lexical_cast<int>(row["y"]);
            job.z = lexical_cast<int>(row["z"]);
            job.id = lexical_cast<int>(row["clientid"]);
            job.style = row["style"];
            jobs.push_back(job);

            // delete that result from the table
            exec(conn, str(format("delete from tasks where gid = %1% and style = %2%") % gid % tx.escape(job.style)));
            // stop listening to that notification
            exec(conn, str(format("unlisten client_%1%_%2%") % gid % safe_identifier(job.style)));

            tx.commit();
         } else {
            // TODO: should this be an *error*?
            std::cerr << "WARN: notified tile with GID " << gid << " and style " << style << ", but couldn't find it in the tasks table.\n";
         }

         // read next notification (if any).
         n = PQnotifies(conn);
      }

      return jobs.size() > 0;
   }

   return false;
}

size_t
pgq_backend::queue_length() const {
   // TODO: implement me.
   return 0;
}

// TODO: move stuff that's in the common backend class into the
// runner / supervisor classes where appropriate.

pgq_runner::pgq_runner(const pt::ptree &pt)
   : impl(new pgq_backend(pt))
{
}

pgq_runner::~pgq_runner()
{
}

void 
pgq_runner::send(const job_t &job) 
{
   impl->send(job);
}

int 
pgq_runner::num_pollitems()
{
   return impl->num_pollitems();
}

void 
pgq_runner::fill_pollitems(zmq::pollitem_t *items) 
{
   impl->fill_pollitems(items);
}

bool 
pgq_runner::handle_pollitems(zmq::pollitem_t *items, std::list<job_t> &jobs)
{
   return impl->handle_pollitems(items, jobs);
}

size_t 
pgq_runner::queue_length() const 
{
   return impl->queue_length();
}

pgq_supervisor::pgq_supervisor(const pt::ptree &pt)
   : impl(new pgq_backend(pt)) 
{
}

pgq_supervisor::~pgq_supervisor() 
{
}

job_t
pgq_supervisor::get_job() 
{
   return impl->get_job();
}

void
pgq_supervisor::notify(const job_t &job)
{
   impl->notify(job);
}

} // namespace dqueue
