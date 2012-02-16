/*------------------------------------------------------------------------------
 *
 *  Logger which outputs all messages to stdout.
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

#ifndef LOGGING_STDOUT_LOGGER_HPP
#define LOGGING_STDOUT_LOGGER_HPP

#include "logger.hpp"

namespace rendermq {

/* this is the default logger, which just does the simplest thing
 * and sends everything to stdout, prefixed with the current time
 * and the message severity level.
 */
class stdout_logger
   : public logger
{
public:
   void log(log_level::type, const std::string &);
   virtual ~stdout_logger();
};

} // namespace rendermq

#endif /* LOGGING_STDOUT_LOGGER_HPP */
