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

#include <cstring>
#include <iostream>
#include <cstdlib>
#include "zmq_utils.hpp"

namespace rendermq {

bool send (zmq::socket_t & socket, std::string const& string) 
{
    zmq::message_t message(string.size());
    std::memcpy(message.data(), string.data(), string.size());
    bool rc = socket.send(message);
    return (rc);
}

bool sendmore (zmq::socket_t & socket, std::string const& string) 
{

    zmq::message_t message(string.size());
    std::memcpy(message.data(), string.data(), string.size());
    bool rc = socket.send(message, ZMQ_SNDMORE);
    return (rc);
}

void zmq_check_version_ok() 
{
   int major, minor, patch;
   zmq_version(&major, &minor, &patch);
   bool ok = ((major == ZMQ_VERSION_MAJOR) &&
              (minor == ZMQ_VERSION_MINOR) &&
              (patch == ZMQ_VERSION_PATCH));

   if (!ok)
   {
      std::cerr << "ERROR! 0MQ version we compiled with is "
                << ZMQ_VERSION_MAJOR << "." << ZMQ_VERSION_MINOR << "." << ZMQ_VERSION_PATCH 
                << " but the version loaded at runtime is "
                << major << "." << minor << "." << patch 
                << ". These versions should not be different. "
                << "Please check your LD_LIBRARY_PATH environment variable."
                << std::endl;
      exit(EXIT_FAILURE);
   }
}

}
