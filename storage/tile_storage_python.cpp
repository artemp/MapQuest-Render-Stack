/*------------------------------------------------------------------------------
 *
 *  Asynchronous tile renderering storage interface.
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

#include "tile_storage.hpp"

using namespace boost::python;
using rendermq::tile_storage;
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

tile_storage *create_from_factory(const dict &d) {
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

  return rendermq::get_tile_storage(pt);
}

string handle_get_data(boost::shared_ptr<tile_storage::handle> h) {
  string s;
  h->data(s);
  return s;
}

object storage_get_meta(tile_storage &ts, const rendermq::tile_protocol &tile) 
{
   object obj;
   string data;
   if (ts.get_meta(tile, data)) 
   {
      obj = str(data);
   }
   return obj;
}

} // anonymous namespace

BOOST_PYTHON_MODULE(tile_storage) {
  class_<tile_storage::handle, 
         boost::shared_ptr<tile_storage::handle>,
         boost::noncopyable>("TileHandle", no_init)
    .def("exists", &tile_storage::handle::exists)
    .def("last_modified", &tile_storage::handle::last_modified)
    .def("data", &handle_get_data)
    .def("expired", &tile_storage::handle::expired)
    ;

  class_<tile_storage,
         boost::noncopyable>("TileStorage", no_init)
    .def("get", &tile_storage::get)
    .def("get_meta", &storage_get_meta)
    .def("put_meta", &tile_storage::put_meta)
    .def("expire", &tile_storage::expire)
    .def("__init__", make_constructor(create_from_factory))
    ;
}
