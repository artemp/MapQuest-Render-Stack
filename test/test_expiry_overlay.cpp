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
#include "storage/expiry_overlay.hpp"
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
#include <boost/property_tree/ptree.hpp>
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

using rendermq::expiry_overlay;
using rendermq::expiry_service;
using rendermq::null_storage;
using rendermq::tile_storage;
using rendermq::tile_protocol;

namespace pt = boost::property_tree;

namespace 
{

// a handle which throws an error if the expiry information 
// is requested. since this should be provided by the 
// overlay, the expired() function on this handle should 
// never be called.
class expiry_throws_handle
   : public tile_storage::handle
{
public:
   bool exists() const { return true; }
   time_t last_modified() const { std::time_t t = std::time(NULL); return t; }
   bool data(string &str) const { str = ""; return true; }
   bool expired() const 
   { 
      throw runtime_error("Expiry information should be provided by the overlay."); 
   }
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

class expiry_throws_storage
   : public tile_storage
{
public:
   shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const
   {
      return shared_ptr<tile_storage::handle>(new null_handle());
   }

   bool get_meta(const tile_protocol &tile, string &str) const
   {
      return false;
   }

   bool put_meta(const tile_protocol &tile, const string &str) const
   {
      return false;
   }

   bool expire(const tile_protocol &tile) const
   {
      throw runtime_error("Expiry information should be provided by the overlay."); 
   }
};

} // anonymous namespace

void test_expiry_intercepts() 
{
   pt::ptree conf;
   shared_ptr<tile_storage> storage(new expiry_throws_storage());
   shared_ptr<expiry_service> service(new rendermq::expiry_service(conf));
   expiry_overlay overlay(storage, service);
   
   tile_protocol tile;
   shared_ptr<rendermq::tile_storage::handle> handle = overlay.get(tile);
}

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Expiry Overlay ==" << endl << endl;

   tests_failed += test::run("test_expiry_intercepts", &test_expiry_intercepts);
   //tests_failed += test::run("test_", &test_);
   
   cout << " >> Tests failed: " << tests_failed << endl << endl;
   
   return 0;
}
