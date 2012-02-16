/*------------------------------------------------------------------------------
 *
 *  Implementation of the interface for the logging framework.
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

#include "logger.hpp"
#include "stdout_logger.hpp"
#include <mapnik/utils.hpp>
#include <map>

using std::map;
using std::string;

namespace rendermq {

logger::~logger() {}

/* factory / singleton for accessing logging functionality.
 */
class logger_factory
   : public mapnik::singleton<logger_factory, mapnik::CreateStatic>,
     private boost::noncopyable
{
public:
   friend class mapnik::CreateStatic<logger_factory>;

   bool add(const string &type, logger_creator func);

   logger *create(const boost::property_tree::ptree &) const;
   
private:
   map<string, logger_creator> m_creators;
};

bool logger_factory::add(const string &type, logger_creator func)
{
   using std::make_pair;
   std::pair<map<string, logger_creator>::iterator, bool> result =
      m_creators.insert(make_pair(type, func));
   return result.second;
}

logger *
logger_factory::create(const boost::property_tree::ptree &conf) const
{
   boost::optional<string> type = conf.get_optional<string>("type");
   if (type)
   {
      map<string, logger_creator>::const_iterator itr = m_creators.find(*type);
      if (itr != m_creators.end())
      {
         return (itr->second)(conf);
      }
   }
   return 0;
}
   
void log::finer(const boost::format &fmt)   { log::instance()->m_logger->log(log_level::finer,   fmt.str()); }
void log::info(const boost::format &fmt)    { log::instance()->m_logger->log(log_level::info,    fmt.str()); }
void log::debug(const boost::format &fmt)   { log::instance()->m_logger->log(log_level::debug,   fmt.str()); }
void log::warning(const boost::format &fmt) { log::instance()->m_logger->log(log_level::warning, fmt.str()); }
void log::error(const boost::format &fmt)   { log::instance()->m_logger->log(log_level::error,   fmt.str()); }

void log::finer(const std::string &msg)   { log::instance()->m_logger->log(log_level::finer,   msg); }
void log::info(const std::string &msg)    { log::instance()->m_logger->log(log_level::info,    msg); }
void log::debug(const std::string &msg)   { log::instance()->m_logger->log(log_level::debug,   msg); }
void log::warning(const std::string &msg) { log::instance()->m_logger->log(log_level::warning, msg); }
void log::error(const std::string &msg)   { log::instance()->m_logger->log(log_level::error,   msg); }

void log::configure(const boost::property_tree::ptree &conf) 
{
   boost::scoped_ptr<logger> new_logger(create_logger(conf));
   if (new_logger) 
   {
      instance()->m_logger.swap(new_logger);
      finer("Logging reconfigured.");
   }
   else
   {
      warning("Logging could not be reconfigured.");
   }
}
 
log::log() 
   : m_logger(new stdout_logger())
{
}

bool register_logger(const string &type, logger_creator func)
{
   return logger_factory::instance()->add(type, func);
}

logger *create_logger(const boost::property_tree::ptree &conf)
{
   return logger_factory::instance()->create(conf);
}

} // namespace rendermq
