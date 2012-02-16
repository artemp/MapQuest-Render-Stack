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

#include "zstream.hpp"
#include "test/common.hpp"

#include <stdexcept>
#include <iostream>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <limits>

using boost::function;
using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::numeric_limits;

namespace {

template <typename T>
struct test_base : private boost::noncopyable {
  test_base();

  void operator()();

  zmq::context_t context;
  zstream::socket::pair socket_a, socket_b;

  boost::mt19937 prng;
  boost::uniform_int<T> dist;
  boost::variate_generator<boost::mt19937 &, boost::uniform_int<T> > rand;

};

template <typename T>
test_base<T>::test_base() 
  : context(1),
    socket_a(context),
    socket_b(context),
    prng(),
    dist(numeric_limits<T>::min(),
         numeric_limits<T>::max()),
    rand(prng, dist) {
  socket_a.bind("inproc://test_base");
  socket_b.connect("inproc://test_base");
}

template <typename T>
void 
test_base<T>::operator()() {
  for (size_t i = 0; i < 100000; ++i) {
    T y, x = rand();
    socket_a << x;
    socket_b >> y;
    if (x != y) {
      throw runtime_error("Failed to send number across pipe.");
    }
  }
}

struct test_uint32 : public test_base<uint32_t> {
};

struct test_uint64 : public test_base<uint64_t> {
};

} // anonymous namespace

int main() {
  int tests_failed = 0;

  cout << "== Testing ZStream Wrapper ==" << endl << endl;

  {
    test_uint32 test;
    tests_failed += test::run("test_uint32", boost::ref(test));
  }
  {
    test_uint64 test;
    tests_failed += test::run("test_uint64", boost::ref(test));
  }
  //tests_failed += test::run("test_", &test_);

  cout << " >> Tests failed: " << tests_failed << endl << endl;

  return 0;
}
