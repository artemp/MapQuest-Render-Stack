/*------------------------------------------------------------------------------
 *
 *  Logger which outputs all messages to a file.
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

#include "file_logger.hpp"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/optional.hpp>
#include <sstream>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <errno.h>

using boost::optional;
using std::string;
using std::ostringstream;
namespace bt = boost::posix_time;
namespace pt = boost::property_tree;

namespace {

rendermq::logger *create_file_logger(const boost::property_tree::ptree &conf) 
{
   return new rendermq::file_logger(conf);
}

static const bool registered = rendermq::register_logger("file", create_file_logger);

}

namespace rendermq {

file_logger::file_logger(const pt::ptree &conf)
   : m_log_fh(NULL)
{
   optional<string> file_name = conf.get_optional<string>("location");
   if (file_name) 
   {
      m_log_fh = fopen(file_name.get().c_str(), "a");

      if (m_log_fh == NULL)
      {
         throw std::runtime_error((boost::format("Cannot open file at %1% because %2%.") 
                                   % file_name.get() % strerror(errno)).str());
      }
   }
   else 
   {
      throw std::runtime_error("Location parameter not provided for file logging target.");
   }
}

file_logger::~file_logger()
{
   if (m_log_fh != NULL)
   {
      fclose(m_log_fh);
   }
}

void
file_logger::log(log_level::type level, const std::string &msg)
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
   fprintf(m_log_fh, "%s", ostr.str().c_str());
   fflush(m_log_fh);
}

} // namespace rendermq
