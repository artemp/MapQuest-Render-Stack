/*------------------------------------------------------------------------------
 *
 *  ZMQ utils
 *
 *  Author: artem@mapnik-consulting.com
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


#ifndef ZMQ_UTILS_HPP
#define ZMQ_UTILS_HPP

#include <string>
#include <zmq.hpp>

namespace rendermq  {

bool send (zmq::socket_t & socket, const std::string & string);
bool sendmore (zmq::socket_t & socket, const std::string & string);

// check that the version of 0MQ that we compiled against is the 
// same as the version we've just linked against. aborts the program
// if there is a mismatch.
void zmq_check_version_ok();

}

#endif // ZMQ_UTILS_HPP
