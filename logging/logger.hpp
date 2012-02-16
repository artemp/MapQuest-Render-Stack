/*------------------------------------------------------------------------------
 *
 *  Header and interface for the logging framework.
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

#ifndef LOGGING_LOGGER_HPP
#define LOGGING_LOGGER_HPP

#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>
#include <boost/scoped_ptr.hpp>
#include <mapnik/utils.hpp>
#include <string>

namespace rendermq {

namespace log_level {
enum type 
{
   finer   = 0,
   debug   = 1,
   info    = 2,
   warning = 3,
   error   = 4
};
}

/* base of all logger types. loggers are created by a factory function
 * and accessed entirely through this interface.
 */
class logger
   : private boost::noncopyable 
{
public:
   // output a log message at the given log level
   virtual void log(log_level::type, const std::string &) = 0;
   virtual ~logger();
};

class log
   : public mapnik::singleton<log, mapnik::CreateStatic>,
     private boost::noncopyable
{
public:
   friend class mapnik::CreateStatic<log>;

   // utility methods to do logging
   static void finer(const boost::format &);
   static void debug(const boost::format &);
   static void info(const boost::format &);
   static void warning(const boost::format &);
   static void error(const boost::format &);

   static void finer(const std::string &);
   static void debug(const std::string &);
   static void info(const std::string &);
   static void warning(const std::string &);
   static void error(const std::string &);

   // to configure the log
   static void configure(const boost::property_tree::ptree &);

   log();

private:
   boost::scoped_ptr<logger> m_logger;
};

// factory function to create loggers. if you implement one of these, register
// it statically with the register_logger() function below.
typedef logger *(*logger_creator)(const boost::property_tree::ptree &);

// call this function from the logger implementation to register the 
// creation function with the singleton factory.
bool register_logger(const std::string &type, logger_creator func);

// create a logger instance from the given configuration. note that 
// the caller owns the returned pointer.
logger *create_logger(const boost::property_tree::ptree &);

} // namespace rendermq

// for convenience, some macros to make logging prettier.
#ifdef RENDERMQ_DEBUG
#define LOG_FINER(x) ::rendermq::log::finer(x)
#define LOG_DEBUG(x) ::rendermq::log::debug(x)
#else /* RENDERMQ_DEBUG */
// don't even generate code for debug logging if debug mode
// isn't turned on...
#define LOG_FINER(x)
#define LOG_DEBUG(x)
#endif /* RENDERMQ_DEBUG */

#define LOG_INFO(x) ::rendermq::log::info(x)
#define LOG_WARNING(x) ::rendermq::log::warning(x)
#define LOG_ERROR(x) ::rendermq::log::error(x)

#endif /* LOGGING_LOGGER_HPP */
