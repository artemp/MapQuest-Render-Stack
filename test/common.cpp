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

#include "common.hpp"
#include "../logging/logger.hpp"
#include <boost/property_tree/ptree.hpp>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <fstream>

using boost::function;
using std::runtime_error;
using std::exception;
using std::cout;
using std::clog;
using std::cerr;
using std::endl;
using std::setw;
using std::flush;
using std::string;
using std::streambuf;
using std::ofstream;
using std::ostream;

#define TEST_NAME_WIDTH (35)

namespace {

void run_with_output_redirected_to(const string &name, function<void ()> test) {
  // create a file for the output to go to.
  string file_name = string("log/") + name + ".testlog";
  boost::property_tree::ptree conf;
  conf.put("type", "file");
  conf.put("location", file_name);
  rendermq::log::configure(conf);

  // run the test
  test();
}

}

namespace test {

int run(const string &name, function<void ()> test) {
  cout << setw(TEST_NAME_WIDTH) << name << flush;
  try {
    run_with_output_redirected_to(name, test);
    cout << "  [PASS]" << endl;
    return 0;

  } catch (const exception &ex) {
    cout << "  [FAIL: " << ex.what() << "]" << endl;
    return 1;

  } catch (...) {
    cerr << "  [FAIL: Unexpected error]" << endl;
    throw;
  }
}

}
