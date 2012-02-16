/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
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

#ifndef DISTRIBUTED_QUEUE_CONFIG_HPP
#define DISTRIBUTED_QUEUE_CONFIG_HPP

#include <string>
#include <map>
#include <list>
#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>

namespace dqueue {

namespace util {
/* Creates a text format UUID.
 */
std::string make_uuid();
}

/* Configuration parsing and utility methods for distributed 
 * queue code. Previously this was scattered around the 
 * initialisation parts of many different bits of code. To 
 * ensure compatibility and make it easier to look up what 
 * configuration the code actually looks at, I'm starting to
 * centralise much of it here.
 */
namespace conf {

/* Represents the parsed section of the distributed queue config
 * file corresponding to a single broker.
 */
struct broker {
  broker(const boost::property_tree::ptree &);
  std::string in_req, in_sub, out_req, out_sub, monitor;
  boost::optional<std::string> in_identity, out_identity;
};

/* Represents the parsed distributed queue config file, containing
 * the broker sections and utility methods for accessing config
 * parameters across all the brokers.
 */
struct common {
  common(const boost::property_tree::ptree &);
  std::map<std::string, broker> brokers;

  std::list<std::string> all_in_req() const;
  std::list<std::string> all_in_sub() const;
  std::list<std::string> all_out_req() const;
  std::list<std::string> all_out_sub() const;
};

} // namespace conf
} // namespace dqueue

#endif /* DISTRIBUTED_QUEUE_CONFIG_HPP */
