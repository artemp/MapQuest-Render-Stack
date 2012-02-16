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
#include "storage/union_storage.hpp"
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

using rendermq::union_storage;
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
   union_storage::list_of_storage_t storages;

private:
   boost::mt19937 prng;
   boost::uniform_int<int> dist;
   boost::variate_generator<boost::mt19937 &, boost::uniform_int<int> > rand;
};

randomized_tester::randomized_tester()
   : storages(), prng(), 
     dist(numeric_limits<int>::min(), numeric_limits<int>::max()), 
     rand(prng, dist)
{
}

void randomized_tester::operator()()
{
   union_storage storage(storages);
   for (int i = 0; i < 100000; ++i) 
   {
      // random zoom level from 0 to 19
      int z = rand() % 19;
      // random tiles within the allowed zoom range (not that it
      // really matters for this test...)
      int x = rand() & ((1 << z) - 1);
      int y = rand() & ((1 << z) - 1);

      tile_protocol tile(rendermq::cmdRender, x, y, z, 0, "style", rendermq::fmtPNG, 0, 0);

      test(tile, storage);
   }

   finally();
}

class test_union_of_nulls_is_null
   : public randomized_tester
{
public:
   test_union_of_nulls_is_null() : randomized_tester()
   {
      storages.push_back(shared_ptr<tile_storage>(new null_storage()));
      storages.push_back(shared_ptr<tile_storage>(new null_storage()));
   }

   void test(const tile_protocol &t, tile_storage &storage) 
   {
      assert_no_tile(t, storage);
   }
};

class test_empty_union_is_null
   : public randomized_tester
{
public:
   test_empty_union_is_null() : randomized_tester() {}

   void test(const tile_protocol &t, tile_storage &storage) 
   {
      assert_no_tile(t, storage);
   }
};

class test_odd_and_even_is_full
   : public randomized_tester
{
public:
   test_odd_and_even_is_full() : randomized_tester()
   {
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_even_tile)));
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_odd_tile)));
   }

   void test(const tile_protocol &t, tile_storage &storage) 
   {
      assert_exists_tile(t, storage);
      assert_not_expired_tile(t, storage);
   }
};

class test_odd_and_even_pass_thru
   : public randomized_tester
{
public:
   test_odd_and_even_pass_thru() 
      : randomized_tester(),
        record(new recording_storage())
   {
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_even_tile)));
      storages.push_back(shared_ptr<tile_storage>(record));
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_odd_tile)));
   }

   void test(const tile_protocol &t, tile_storage &storage)
   {
      assert_exists_tile(t, storage);
      string dummy;
      if (!storage.get_meta(t, dummy)) 
      {
         throw runtime_error((boost::format("Metatile %1% could not be got.") % t).str());
      }
   }

   void finally() 
   {
      BOOST_FOREACH(const tile_protocol &t, record->gets)
      {
         if (is_even_tile(t)) 
         {
            throw runtime_error((boost::format("Tile %1% is an even tile, and should not have been attempted to be got after the even tile layer in the storage.") % t).str());
         }
      }
   }

private:
   // NOTE: don't need to delete this, as it's done when the storages object
   // in the base class is collected. it's only here so that it can be used
   // in the 'finally' method.
   recording_storage *record;
};

class test_put_puts_to_all
   : public randomized_tester
{
public:
   test_put_puts_to_all() 
      : randomized_tester(),
        record(new recording_storage())
   {
      storages.push_back(shared_ptr<tile_storage>(new null_storage()));
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_even_tile)));
      storages.push_back(shared_ptr<tile_storage>(record));
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_odd_tile)));
   }

   void test(const tile_protocol &t, tile_storage &storage)
   {
      tiles.push_back(t);
      if (storage.put_meta(t, ""))
      {
         throw runtime_error((boost::format("There's a null storage in the chain - put_meta shouldn't return true, ever, even for tile %1%.") % t).str());
      }
   }

   void finally() 
   {
      list<tile_protocol>::iterator itr = tiles.begin();
      list<tile_protocol>::iterator jtr = record->puts.begin();
      for (; (itr != tiles.end()) && (jtr != record->puts.end()); ++itr, ++jtr)
      {
         if (!(*itr == *jtr))
         {
            throw runtime_error((boost::format("Record of tile puts doesn't match: %1% != %2%.") % *itr % *jtr).str());
         }
      }
   }

private:
   // see comment in test_odd_and_even_pass_thru
   recording_storage *record;
   list<tile_protocol> tiles;
};

class test_expire_expires_from_all
   : public randomized_tester
{
public:
   test_expire_expires_from_all()
      : randomized_tester(),
        record(new recording_storage())
   {
      storages.push_back(shared_ptr<tile_storage>(new null_storage()));
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_even_tile)));
      storages.push_back(shared_ptr<tile_storage>(record));
      storages.push_back(shared_ptr<tile_storage>(new predicate_tiles_exist(&is_odd_tile)));
   }

   void test(const tile_protocol &t, tile_storage &storage)
   {
      tiles.push_back(t);
      if (storage.expire(t))
      {
         throw runtime_error((boost::format("There's a null storage in the chain - expire shouldn't return true, ever, even for tile %1%.") % t).str());
      }
   }

   void finally() 
   {
      list<tile_protocol>::iterator itr = tiles.begin();
      list<tile_protocol>::iterator jtr = record->puts.begin();
      for (; (itr != tiles.end()) && (jtr != record->puts.end()); ++itr, ++jtr)
      {
         if (!(*itr == *jtr))
         {
            throw runtime_error((boost::format("Record of tile expiries doesn't match: %1% != %2%.") % *itr % *jtr).str());
         }
      }
   }

private:
   // see comment in test_odd_and_even_pass_thru
   recording_storage *record;
   list<tile_protocol> tiles;
};

} // anonymous namespace

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Union Storage Backend ==" << endl << endl;
   
   {
      test_union_of_nulls_is_null test;
      tests_failed += test::run("test_union_of_nulls_is_null", boost::ref(test));
   }
   {
      test_empty_union_is_null test;
      tests_failed += test::run("test_empty_union_is_null", boost::ref(test));
   }
   {
      test_odd_and_even_is_full test;
      tests_failed += test::run("test_odd_and_even_is_full", boost::ref(test));
   }
   {
      test_odd_and_even_pass_thru test;
      tests_failed += test::run("test_odd_and_even_pass_thru", boost::ref(test));
   }
   {
      test_put_puts_to_all test;
      tests_failed += test::run("test_put_puts_to_all", boost::ref(test));
   }
   {
      test_expire_expires_from_all test;
      tests_failed += test::run("test_expire_expires_from_all", boost::ref(test));
   }
   //tests_failed += test::run("test_", &test_);
   
   cout << " >> Tests failed: " << tests_failed << endl << endl;
   
   return 0;
}
