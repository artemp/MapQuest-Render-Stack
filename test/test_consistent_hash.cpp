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

#include "dqueue/consistent_hash.hpp"
#include "tile_protocol.hpp"
#include "test/common.hpp"
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <map>
#include <limits>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <sstream>

using rendermq::consistent_hash;
using boost::function;
using boost::optional;
using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::numeric_limits;

namespace {

template <class HashType>
void assert_lookup(const HashType &h, 
                   const typename HashType::key_type &k, 
                   const typename HashType::value_type &v) {
   optional<typename HashType::value_type> v2 = h.lookup(k);
   if (!v2) { throw runtime_error("Expected value not found in hash lookup."); }
   if (v2.get() != v) { throw runtime_error("Looked-up value different from expected."); }
}
}

/* check that if there are no entries then the hash returns no
 * results. seems pretty obvious, but worth checking.
 */
void test_zero_entry() {
   consistent_hash<string, string> h(100);

   if (h.lookup("12345")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("12346")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("12347")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("12348")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("blash")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("!")) { throw runtime_error("Unexpected item found in empty hash."); }
   if (h.lookup("  ")) { throw runtime_error("Unexpected item found in empty hash."); }
}

/* with a single entry that should be returned for all requests.
 */
void test_single_entry() {
   consistent_hash<string, string> h(100);

   h.insert("foobar");
  
   assert_lookup(h, "12345", "foobar");
   assert_lookup(h, "12346", "foobar");
   assert_lookup(h, "12347", "foobar");
   assert_lookup(h, "12348", "foobar");
   assert_lookup(h, "blahblah", "foobar");
   assert_lookup(h, "", "foobar");
   assert_lookup(h, "sjhkjfd", "foobar");
   assert_lookup(h, "!", "foobar");
}

/* check that after adding and removing a single entry that it
 * acts like an empty hash again. i.e: that everything is being
 * removed correctly.
 */
void test_add_remove_entry() {
   consistent_hash<string, string> h(100);

   h.insert("foobar");
  
   assert_lookup(h, "12345", "foobar");
   assert_lookup(h, "blahblah", "foobar");
   assert_lookup(h, "", "foobar");
   assert_lookup(h, "!", "foobar");

   h.erase("foobar");

   if (h.lookup("12345")) { throw runtime_error("Unexpected item found in supposedly empty hash."); }
   if (h.lookup("blahblah")) { throw runtime_error("Unexpected item found in supposedly empty hash."); }
   if (h.lookup("")) { throw runtime_error("Unexpected item found in supposedly empty hash."); }
   if (h.lookup("!")) { throw runtime_error("Unexpected item found in supposedly empty hash."); }
}

/* on average, lookups should be roughly evenly spread across the values.
 */
void test_average_performance() {
   const size_t num_entries = 100000;
   const size_t num_reps = 1000;

   map<string, size_t> counts;
   counts["foobar"] = 0;
   counts["barbaz"] = 0;
   counts["bazbat"] = 0;
   counts["batfoo"] = 0;

   consistent_hash<size_t, string> h(num_reps);

   for (map<string, size_t>::iterator itr = counts.begin();
        itr != counts.end(); ++itr) {
      h.insert(itr->first);
   }

   for (size_t i = 0; i < num_entries; ++i) {
      optional<string> val = h.lookup(i);
      if (!val) { throw runtime_error("Got empty value from hash with inserted values."); }
      map<string, size_t>::iterator citr = counts.find(val.get());
      if (citr == counts.end()) { throw runtime_error("Got value from hash that hadn't been inserted."); }
      citr->second++;
   }

   double sum = 0.0, max = 0.0, min = numeric_limits<double>::max();
   for (map<string, size_t>::iterator itr = counts.begin();
        itr != counts.end(); ++itr) {
      sum += itr->second;
      if (max < itr->second) { max = itr->second; }
      if (min > itr->second) { min = itr->second; }
   }
   double unevenness = (max - min) / (sum / counts.size());
   double expected = sqrt(double(num_reps) / double(num_entries));

   if (unevenness >= expected) { 
      std::ostringstream ostr;
      ostr << "Distribution of entries is unexpectedly large (" << unevenness << " >= " << expected << ").";
      throw std::runtime_error(ostr.str());
   }
}

void check_hashing_job(int x, int y, int z, consistent_hash<rendermq::tile_protocol, int> &h) {
   rendermq::tile_protocol job;

   // strip last 3 bits
   int base_x = x & ~7;
   int base_y = y & ~7;
   job.x = base_x; job.y = base_y; job.z = z;

   int intended_target = h.lookup(job).get();
   for (int j = base_y; j < base_y + 8; ++j) {
      for (int i = base_x; i < base_x + 8; ++i) {
         job.x = i; job.y = j;
         int target = h.lookup(job).get();
         if (target != intended_target) {
            throw runtime_error((boost::format("Missed intended target. Expected %1%, got %2%") % intended_target % target).str());
         }
      }
   }
}

void test_hashing_job() {
   consistent_hash<rendermq::tile_protocol, int> h(100);

   // add lots of values to make it more likely that one will
   // hit one of these if it misses.
   for (int i = 0; i < 100; ++i) {
      h.insert(i);
   }

   check_hashing_job(4551, 6167, 14, h);
   check_hashing_job(1000, 1000, 11, h);
   check_hashing_job(0, 0, 3, h);
}

int main() {
   int tests_failed = 0;

   cout << "== Testing Consistent Hash ==" << endl << endl;

   tests_failed += test::run("test_zero_entry", &test_zero_entry);
   tests_failed += test::run("test_single_entry", &test_single_entry);
   tests_failed += test::run("test_add_remove_entry", &test_add_remove_entry);
   tests_failed += test::run("test_average_performance", &test_average_performance);
   tests_failed += test::run("test_hashing_job", &test_hashing_job);
   //tests_failed += test::run("test_", &test_);

   cout << " >> Tests failed: " << tests_failed << endl << endl;

   return 0;
}
