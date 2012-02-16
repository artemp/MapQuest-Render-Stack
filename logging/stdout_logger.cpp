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

#include "stdout_logger.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <sstream>
#include <cstdio>

using std::ostringstream;
namespace bt = boost::posix_time;

namespace {

rendermq::logger *create_stdout_logger(const boost::property_tree::ptree &) 
{
   return new rendermq::stdout_logger();
}

static const bool registered = rendermq::register_logger("stdout", create_stdout_logger);

}

namespace rendermq {

void
stdout_logger::log(log_level::type level, const std::string &msg)
{
   ostringstream ostr;
   ostr << bt::microsec_clock::local_time() << " ";
   if (level == log_level::finer) {
      ostr << "[FINER] ";
   } else if (level == log_level::debug) {
      ostr << "[DEBUG] ";
   } else if (level == log_level::info) {
      ostr << "[INFO]  ";
   } else if (level == log_level::warning) {
      ostr << "[WARN]  ";
   } else {
      ostr << "[ERROR] ";
   }
   ostr << msg << "\n";
   fprintf(stdout, "%s", ostr.str().c_str());
   fflush(stdout);
}

stdout_logger::~stdout_logger()
{
}

} // namespace rendermq
