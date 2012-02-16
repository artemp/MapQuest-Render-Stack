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

#include "storage/null_storage.hpp"
#include "storage/per_style_storage.hpp"
#include "test/common.hpp"

#include <stdexcept>
#include <iostream>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <limits>

using boost::function;
using boost::shared_ptr;
using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::numeric_limits;
using std::time_t;
using std::list;
using std::vector;
using std::make_pair;

using rendermq::per_style_storage;
using rendermq::null_storage;
using rendermq::tile_storage;
using rendermq::tile_protocol;

namespace 
{

void assert_no_tile(const tile_protocol &t, const tile_storage &store)
{
   shared_ptr<tile_storage::handle> handle = store.get(t);
   if (handle->exists())
   {
      throw runtime_error((boost::format("Storage should not have tile %1%, but does.") % t).str());
   }
}

void assert_exists_tile(const tile_protocol &t, const tile_storage &store)
{
   shared_ptr<tile_storage::handle> handle = store.get(t);
   if (!handle->exists())
   {
      throw runtime_error((boost::format("Storage should have tile %1%, but does not.") % t).str());
   }
}

void assert_not_expired_tile(const tile_protocol &t, const tile_storage &store)
{
   shared_ptr<tile_storage::handle> handle = store.get(t);
   if (!handle->exists() || handle->expired())
   {
      throw runtime_error((boost::format("Storage should have tile %1% and it not be expired.") % t).str());
   }
}

class exists_handle
   : public tile_storage::handle
{
public:
   bool exists() const { return true; }
   time_t last_modified() const { std::time_t t = std::time(NULL); return t; }
   bool data(string &str) const { str = ""; return true; }
   bool expired() const { return false; }
};

class null_handle 
   : public tile_storage::handle 
{
public:
   bool exists() const { return false; }
   time_t last_modified() const { return time_t(0); }
   bool data(string &) const { return false; }
   bool expired() const { return false; }
};

// even numbered tiles (x + y) & 1 == 0 exist, others are null
class predicate_tiles_exist
   : public tile_storage
{
public:
   predicate_tiles_exist(boost::function<bool (const tile_protocol &)> pred)
      : m_pred(pred) 
   {
   }

   shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const
   {
      if (m_pred(tile)) 
      {
         return shared_ptr<tile_storage::handle>(new exists_handle());
      }
      else
      {
         return shared_ptr<tile_storage::handle>(new null_handle());
      }
   }

   bool get_meta(const tile_protocol &tile, string &str) const
   {
      if (m_pred(tile))
      {
         str = "";
         return true;
      } 
      else
      { 
         return false;
      }
   }

   bool put_meta(const tile_protocol &tile, const string &str) const
   {
      return m_pred(tile);
   }

   bool expire(const tile_protocol &tile) const
   {
      return m_pred(tile);
   }

private:
   boost::function<bool (const tile_protocol &)> m_pred;
};

bool is_even_tile(const tile_protocol &tile) 
{
   return ((tile.x + tile.y) & 1) == 0;
}

bool is_odd_tile(const tile_protocol &tile) 
{
   return ((tile.x + tile.y) & 1) == 1;
}

bool always_true(const tile_protocol &tile)
{
   return true;
}

/* a storage which has no tiles, but records all attempts to get or put
 * data to or from the storage. this is useful to check that things were
 * routed to the right place.
 */
class recording_storage
   : public tile_storage
{
public:
   shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const
   {
      gets.push_back(tile);
      return shared_ptr<tile_storage::handle>(new null_handle());
   }

   bool get_meta(const tile_protocol &tile, string &str) const
   {
      gets.push_back(tile);
      return false;
   }

   bool put_meta(const tile_protocol &tile, const string &str) const
   {
      puts.push_back(tile);
      return false;
   }

   bool expire(const tile_protocol &tile) const
   {
      puts.push_back(tile);
      return false;
   }

   mutable list<tile_protocol> gets, puts;
};

class randomized_tester
{
public:
   randomized_tester();
   virtual ~randomized_tester() {}
   virtual void test(const tile_protocol &, tile_storage &) = 0;
   void operator()();
   virtual void finally() {}

protected:
   per_style_storage::map_of_storage_t storages;
   boost::shared_ptr<tile_storage> default_storage;
   vector<string> style_names;

private:
   boost::mt19937 prng;
   boost::uniform_int<int> dist;
   boost::variate_generator<boost::mt19937 &, boost::uniform_int<int> > rand;
};

randomized_tester::randomized_tester()
   : storages(), default_storage(), style_names(), prng(), 
     dist(numeric_limits<int>::min(), numeric_limits<int>::max()), 
     rand(prng, dist)
{
}

void randomized_tester::operator()()
{
   per_style_storage storage(storages, default_storage);
   for (int i = 0; i < 100000; ++i) 
   {
      // random zoom level from 0 to 19
      int z = rand() % 19;
      // random tiles within the allowed zoom range (not that it
      // really matters for this test...)
      int x = rand() & ((1 << z) - 1);
      int y = rand() & ((1 << z) - 1);

      // get a random style
      if (style_names.empty()) { throw std::runtime_error("randomized_tester::style_names should not be empty in test."); }
      const string &style = style_names[rand() % style_names.size()];

      tile_protocol tile(rendermq::cmdRender, x, y, z, 0, style, rendermq::fmtPNG, 0, 0);

      test(tile, storage);
   }

   finally();
}

class test_style_routing
   : public randomized_tester
{
public:
   test_style_routing() 
      : randomized_tester(),
        foo_record(new recording_storage()),
        bar_record(new recording_storage()),
        def_record(new recording_storage())
   {
      storages.insert(make_pair("foo", shared_ptr<tile_storage>(foo_record)));
      storages.insert(make_pair("bar", shared_ptr<tile_storage>(bar_record)));
      default_storage = shared_ptr<tile_storage>(def_record);
      style_names.push_back("foo");
      style_names.push_back("bar");
      style_names.push_back("other");
   }

   void test(const tile_protocol &t, tile_storage &storage) 
   {
      // none of the recording storages claim to have tiles.
      assert_no_tile(t, storage);
   }

   void finally() 
   {
      BOOST_FOREACH(const tile_protocol &t, foo_record->gets)
      {
         if (t.style != "foo")
         {
            throw runtime_error((boost::format("Tile %1% should have style 'foo', but has '%2%' instead.") % t % t.style).str());
         }
      }
      BOOST_FOREACH(const tile_protocol &t, bar_record->gets)
      {
         if (t.style != "bar")
         {
            throw runtime_error((boost::format("Tile %1% should have style 'bar', but has '%2%' instead.") % t % t.style).str());
         }
      }
      BOOST_FOREACH(const tile_protocol &t, def_record->gets)
      {
         if (t.style != "other")
         {
            throw runtime_error((boost::format("Tile %1% should have style 'other', but has '%2%' instead.") % t % t.style).str());
         }
      }
   }

private:
   // see comment in test_odd_and_even_pass_thru
   recording_storage *foo_record, *bar_record, *def_record;
};

} // anonymous namespace

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Per-Style Storage Backend ==" << endl << endl;
   
   {
      test_style_routing test;
      tests_failed += test::run("test_style_routing", boost::ref(test));
   }
   //tests_failed += test::run("test_", &test_);
   
   cout << " >> Tests failed: " << tests_failed << endl << endl;
   
   return 0;
}
