/*------------------------------------------------------------------------------
 *
 *  Python wrapper for logging library.
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

#include <boost/python.hpp>
#include <boost/noncopyable.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <string>

#include "logger.hpp"

using namespace boost::python;
using std::string;

namespace {

template<typename T>
bool try_set(boost::property_tree::ptree &pt, const string &key, object &val) {
  extract<T> ex(val);
  if (ex.check()) {
    pt.put<T>(key, ex());
    return true;
  }
  return false;
}

void configure_from_dict(const dict &d) {
  boost::property_tree::ptree pt;

  boost::python::list keys=d.keys();
  for (int i = 0; i < len(keys); ++i) {
    string key = extract<string>(keys[i]);
    object val = d[key];
    
    try_set<string>(pt, key, val) 
      || try_set<int>(pt, key, val)
      || try_set<double>(pt, key, val)
      ;
  }

  rendermq::log::configure(pt);
}

void configure_from_file(const std::string &filename) {
   try
   {
      boost::property_tree::ptree logging_config;
      boost::property_tree::read_ini(filename, logging_config);
      rendermq::log::configure(logging_config);
   }
   catch (const std::exception &e)
   {
      std::cerr << "Error while setting up logging from: " << filename << "\n";
      throw;
   }
}

} // anonymous namespace

BOOST_PYTHON_MODULE(mq_logging) 
{
   // resolve the overloads - don't want to expose boost::format to python,
   // as it's already quite capable of doing its own string interpolation.
   void (*finer)  (const string &) = &rendermq::log::finer;
   void (*debug)  (const string &) = &rendermq::log::debug;
   void (*info)   (const string &) = &rendermq::log::info;
   void (*warning)(const string &) = &rendermq::log::warning;
   void (*error)  (const string &) = &rendermq::log::error;

   // note we're not exposing log as a class - no need, since all the
   // access methods are static.
   def("finer",   finer);
   def("debug",   debug);
   def("info",    info);
   def("warning", warning);
   def("error",   error);
   def("configure", &configure_from_dict);
   def("configure_file", &configure_from_file);
}
