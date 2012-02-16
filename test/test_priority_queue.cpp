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

#include "task_queue.hpp"
#include "test/common.hpp"
#include "logging/logger.hpp"
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <set>
#include <list>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

using rendermq::task_queue;
using rendermq::tile_protocol;
using rendermq::task;
using boost::optional;
using boost::function;
using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::distance;
using std::pair;
using std::set;
using std::list;

using rendermq::cmdIgnore;
using rendermq::cmdDone;
using rendermq::cmdRender;
using rendermq::fmtPNG;

namespace {
/* utility method to check that the front item of a queue is as-expected.
 */
void assert_pop(task_queue &q, const tile_protocol &t) {
  optional<const task &> tsk = q.front();
  if (!tsk) { throw runtime_error("Queue prematurely empty."); }
  const tile_protocol &t2 = static_cast<const tile_protocol &>(tsk.get());
  //cout << " >> Expecting: " << t << endl;
  //cout << " << Actual:    " << t2 << endl;
  if (!(t == t2)) { throw runtime_error("Job at front of queue is different from expected."); }
  q.pop();
}

void assert_subscribers(task_queue &q, const tile_protocol &t, const set<string> &s) {
  optional<const task &> tsk = q.get(t);
  if (!tsk) { throw runtime_error("Queue prematurely empty."); }
  pair<task::iterator, task::iterator> subs = tsk.get().subscribers();
  std::iterator_traits<task::iterator>::difference_type count = distance(subs.first, subs.second);
  if (count < 0) {
    throw runtime_error("Distance from first to last subscriber is negative!");
  }
  if (size_t(count) != s.size()) {
    throw runtime_error("Differing count of subscribers between expected and actual.");
  }
  for (task::iterator itr = subs.first; itr != subs.second; ++itr) {
    if (s.count(itr->second) != 1) {
      throw runtime_error("Subscriber count differs between expected and actual.");
    }
  }
  q.erase(t);
}
}

/* test that the queue returns items in a priority-ordered fashion.
 */
void test_is_priority_queue() {
  task_queue q;

  q.push(tile_protocol(cmdIgnore, 0, 0, 3, 0, "", fmtPNG, 0, 0), "", 100);
  q.push(tile_protocol(cmdIgnore, 0, 0, 4, 0, "", fmtPNG, 1, 0), "",  99);
  q.push(tile_protocol(cmdIgnore, 8, 0, 4, 0, "", fmtPNG, 0, 1), "",  98);
  q.push(tile_protocol(cmdIgnore, 0, 8, 4, 0, "", fmtPNG, 1, 1), "",  97);
  q.push(tile_protocol(cmdIgnore, 0, 0, 5, 0, "", fmtPNG, 2, 0), "",  96);
  q.push(tile_protocol(cmdIgnore, 0, 8, 5, 0, "", fmtPNG, 0, 2), "", 101);
  q.push(tile_protocol(cmdIgnore, 8, 0, 5, 0, "", fmtPNG, 2, 2), "", 102);
  q.push(tile_protocol(cmdIgnore, 8, 8, 5, 0, "", fmtPNG, 3, 3), "",  95);

  // check the size of the queue - we don't want any collapsing in this
  // test, that's for a later test.
  if (q.size() != 8) {
    throw runtime_error("Size of queue should be 8.");
  }

  // check that the elements come off in highest priority first order.
  assert_pop(q, tile_protocol(cmdIgnore, 8, 0, 5, 0, "", fmtPNG, 2, 2));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 8, 5, 0, "", fmtPNG, 0, 2));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 3, 0, "", fmtPNG, 0, 0));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 4, 0, "", fmtPNG, 1, 0));
  assert_pop(q, tile_protocol(cmdIgnore, 8, 0, 4, 0, "", fmtPNG, 0, 1));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 8, 4, 0, "", fmtPNG, 1, 1));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 5, 0, "", fmtPNG, 2, 0));
  assert_pop(q, tile_protocol(cmdIgnore, 8, 8, 5, 0, "", fmtPNG, 3, 3));

  // check that the queue is now empty
  if (q.size() != 0) {
    throw runtime_error("Queue should now be empty.");
  }
}

/* test that the queue collapses tile requests with the same metatile
 * address (i.e: with the non-metatile bits of x & y masked out).
 */
void test_collapses_requests() {
  task_queue q;

  // all these are part of the same metatile
  q.push(tile_protocol(cmdIgnore, 1, 1, 5, 0, "", fmtPNG), "", 100);
  q.push(tile_protocol(cmdIgnore, 2, 2, 5, 0, "", fmtPNG), "", 100);
  q.push(tile_protocol(cmdIgnore, 3, 3, 5, 0, "", fmtPNG), "", 100);
  q.push(tile_protocol(cmdIgnore, 4, 4, 5, 0, "", fmtPNG), "", 100);

  // since there's only one metatile, check that there's only one item
  // in the queue.
  if (q.size() != 1) {
    throw runtime_error("Size of queue should be 1 due to metatile collapsing.");
  }

  // check that the item in the queue is the metatile, which isn't
  // actually one of the requests we put in.
  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 5, 0, "", fmtPNG));

  // check that the queue is now empty
  if (q.size() != 0) {
    throw runtime_error("Queue should now be empty.");
  }
}

/* test that collapsed requests collapse to the *highest* priority.
 */
void test_collapsing_priority() {
  task_queue q;

  q.push(tile_protocol(cmdIgnore, 1, 1, 5, 0, "", fmtPNG), "",  50);
  q.push(tile_protocol(cmdIgnore, 3, 3, 6, 0, "", fmtPNG), "", 100);
  q.push(tile_protocol(cmdIgnore, 4, 4, 6, 0, "", fmtPNG), "",  51);
  q.push(tile_protocol(cmdIgnore, 2, 2, 5, 0, "", fmtPNG), "", 101);
  q.push(tile_protocol(cmdIgnore, 5, 5, 7, 0, "", fmtPNG), "", 102);

  if (q.size() != 3) {
    throw runtime_error("Size of queue should be 3 due to metatile collapsing.");
  }

  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 7, 0, "", fmtPNG));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 5, 0, "", fmtPNG));
  assert_pop(q, tile_protocol(cmdIgnore, 0, 0, 6, 0, "", fmtPNG));

  // check that the queue is now empty
  if (q.size() != 0) {
    throw runtime_error("Queue should now be empty.");
  }
}

/* test that collapsed requests reference their "subscribers", allowing the 
 * completion of a collapsed request to be mapped back to the client which
 * initiated it.
 */
void test_subscribers() {
  task_queue q;

  q.push(tile_protocol(cmdIgnore, 1, 1, 5, 0, "", fmtPNG), "A", 100);
  q.push(tile_protocol(cmdIgnore, 3, 3, 6, 0, "", fmtPNG), "B", 100);
  q.push(tile_protocol(cmdIgnore, 4, 4, 6, 0, "", fmtPNG), "C", 100);
  q.push(tile_protocol(cmdIgnore, 2, 2, 5, 0, "", fmtPNG), "D", 100);

  if (q.size() != 2) {
    throw runtime_error("Size of queue should be 2 due to metatile collapsing.");
  }

  set<string> z5;
  z5.insert("A");
  z5.insert("D");
  
  set<string> z6;
  z6.insert("B");
  z6.insert("C");

  assert_subscribers(q, tile_protocol(cmdIgnore, 0, 0, 5, 0, "", fmtPNG), z5);
  assert_subscribers(q, tile_protocol(cmdIgnore, 0, 0, 6, 0, "", fmtPNG), z6);

  // check that the queue is now empty
  if (q.size() != 0) {
    throw runtime_error("Queue should now be empty.");
  }
}

void test_resubmit()
{
  task_queue q;

  q.push(tile_protocol(cmdRender, 1, 1, 1, 0, "", fmtPNG), "A", 100);
  optional<const task &> tsk = q.front();
  tile_protocol proto = static_cast<tile_protocol>(*tsk);
  q.set_processed(proto);

  usleep(1500000);

  q.resubmit_older_than(1);
  optional<const task &> tsk2 = q.front();
  if (!tsk2) { throw runtime_error("Task not resubmitted"); }
  tile_protocol proto2 = static_cast<tile_protocol>(*tsk2);
  if (proto != proto2) { throw runtime_error("Resubmitted task not equal to original task."); }
  q.set_processed(proto2);

  proto.status = cmdDone;
  optional<const task &> tsk3 = q.get(proto);
  if (!tsk3) { throw runtime_error("Can't get task from 'done' job."); }
  q.erase(proto);

  if (q.size() != 0) {
    throw runtime_error("Queue should now be empty.");
  }
}

void test_collision()
{
   typedef rendermq::task::iterator task_iterator;
   typedef std::pair<task_iterator, task_iterator> task_range;

   list<string> styles;
   const string addr = "";
   task_queue q;

   styles.push_back("osm");
   styles.push_back("map");
   styles.push_back("hyb");

   for (int z = 0; z < 12; ++z) 
   {
      const int coord_end = 1 << z;

      for (int x = 0; x < coord_end; x += 8) 
      {
         for (int y = 0; y < coord_end; y += 8) 
         {
            BOOST_FOREACH(string style, styles)
            {
               tile_protocol t(cmdRender, x, y, z, 0, style, fmtPNG, 0, 0);
               if (!q.push(t, addr, 100)) {
                  optional<const task &> collided_with = q.get(t);
                  if (collided_with)
                  {
                     task_range range = collided_with.get().subscribers();
                     for (task_iterator itr = range.first; itr != range.second; ++itr) {
                        LOG_DEBUG(boost::format("COLLIDE: %1%, addr=%2%") % itr->first % itr->second);
                     }

                     throw std::runtime_error(
                        (boost::format("collision in task queue on tile %1% with %2%") 
                         % t % collided_with.get()).str());
                  }
                  else
                  {
                     throw std::runtime_error("Collision, but I don't know what with! I'm confused.");
                  }
               }
            }
         }
      }
   }
}

void test_front_processing()
{
   task_queue q;
   size_t count_gen = 0, count_proc = 0;
   int z = 18;
   const string style = "map";
   const string addr = "";

   for (int x = 47000; x < 47496; x++) {
      for (int y = 93200; y < 93696; y++) {
         for (int p = 0; p < 101; p += 20) {
            int client_id = (count_gen + p) % 100;
            tile_protocol t(cmdRender, x, y, z, client_id, style, fmtPNG, 0, 0);
            q.push(t, addr, p);
         }
         ++count_gen;
      }
   }

   while (true) {
      boost::optional<const task &> t = q.front();
      if (t) {
         tile_protocol proto = static_cast<tile_protocol>(*t);
         q.set_processed(proto);
         count_proc += 64;

      } else {
         break;
      }
   }

   if (count_proc != count_gen) {
      throw std::runtime_error((boost::format("Difference between number generated (%1%) and number processed (%2%) - error in queue logic!") % count_gen % count_proc).str());
   }
}
      
   
int main() {
  int tests_failed = 0;

  cout << "== Testing Priority Queue ==" << endl << endl;

  tests_failed += test::run("test_is_priority_queue", &test_is_priority_queue);
  tests_failed += test::run("test_collapses_requests", &test_collapses_requests);
  tests_failed += test::run("test_collapsing_priority", &test_collapsing_priority);
  tests_failed += test::run("test_subscribers", &test_subscribers);
  tests_failed += test::run("test_resubmit", &test_resubmit);
  tests_failed += test::run("test_collision", &test_collision);
  tests_failed += test::run("test_front_processing", &test_front_processing);

  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return 0;
}
