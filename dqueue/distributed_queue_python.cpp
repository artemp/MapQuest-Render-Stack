/*------------------------------------------------------------------------------
 *
 *  Asynchronous tile renderering queue backend interface.
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
#include <boost/lexical_cast.hpp>

#include "distributed_queue.hpp"
#include "backend.hpp"

using namespace boost::python;
using dqueue::supervisor;
using rendermq::tile_protocol;
using rendermq::protoCmd;
using rendermq::protoFmt;

namespace 
{
std::string tile_protocol_to_string(const tile_protocol &t)
{
   std::ostringstream ostr;
   ostr << t;
   return ostr.str();
}
}

BOOST_PYTHON_MODULE(dqueue) {
    class_<supervisor, boost::noncopyable>("Supervisor", init<std::string, optional<std::string> >())
        .def("get_job", &supervisor::get_job)
        .def("notify", &supervisor::notify)
        ;

    // we're not using all of these, i'm pretty sure, but seems a good idea
    // to wrap them anyway.
    enum_<protoCmd>("ProtoCommand")
        .value("cmdIgnore", rendermq::cmdIgnore)
        .value("cmdRender", rendermq::cmdRender)
        .value("cmdDirty", rendermq::cmdDirty)
        .value("cmdDone", rendermq::cmdDone)
        .value("cmdNotDone", rendermq::cmdNotDone)
        .value("cmdRenderPrio", rendermq::cmdRenderPrio)
        .value("cmdRenderBulk", rendermq::cmdRenderBulk)
        .value("cmdStatus", rendermq::cmdStatus)
        ;

    enum_<protoFmt>("ProtoFormat")
      .value("fmtPNG", rendermq::fmtPNG)
      .value("fmtJPEG", rendermq::fmtJPEG)
      .value("fmtGIF", rendermq::fmtGIF)
      .value("fmtJSON", rendermq::fmtJSON)
      ;

    class_<tile_protocol>("TileProtocol")
        .def_readwrite("x", &tile_protocol::x)
        .def_readwrite("y", &tile_protocol::y)
        .def_readwrite("z", &tile_protocol::z)
        .def_readwrite("id", &tile_protocol::id)
        .def_readwrite("status", &tile_protocol::status)
        .def_readwrite("style", &tile_protocol::style)
        .def_readwrite("format", &tile_protocol::format)
        .def_readwrite("last_modified", &tile_protocol::last_modified)
        .def("__str__", &tile_protocol_to_string)
        .add_property("data", make_function(&tile_protocol::data,return_value_policy<copy_const_reference>()),
                      &tile_protocol::set_data)
        ;

}
