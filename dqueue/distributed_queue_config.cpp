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

#include "distributed_queue_config.hpp"
#include "../logging/logger.hpp"
#include <uuid/uuid.h>
#include <boost/tokenizer.hpp>
#include <boost/regex.hpp>
#include <stdexcept>

//for host name resolution
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace pt = boost::property_tree;
using std::string;
using std::list;
using std::map;

namespace {

using dqueue::conf::broker;
using dqueue::conf::common;

list<string> parse_list(const string &str) {
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
  boost::char_separator<char> sep(", \t");
  tokenizer_t tok(str, sep);

  list<string> l;
  std::copy(tok.begin(), tok.end(), std::back_inserter(l));
  return l;
}

template <typename T>
list<string> brokers_get(const common &all, T t) {
  list<string> rv;
  transform(all.brokers.begin(), all.brokers.end(), 
            back_inserter(rv), 
            bind(t, 
                 bind(&map<string,broker>::value_type::second, 
                      _1)));
  return rv;
}

string host_to_ip(const string& hostname)
{
   //try to resolve the hostname
   struct hostent *he;
   if((he = gethostbyname(hostname.c_str())) == NULL)
   {
      string error = boost::str(boost::format("Invalid dqueue host (%1%) should look like tcp://[hostname|ip]:port") % hostname);
      LOG_ERROR(error);
      throw std::runtime_error(error.c_str());
   }

   //get the first ip
   struct in_addr** addr_list = (struct in_addr **)he->h_addr_list;
   return inet_ntoa(*addr_list[0]);
}

string parse_zmq_host(const string& zmqHost)
{
   //grab the host/ip portion from things like tcp://mq-devhost-lm04.ihost.aol.com:44444
   boost::regex expression("(tcp://|udp://)([0-9a-zA-Z\\-\\.]+)(:.*)");
   string host = boost::regex_replace(zmqHost, expression, "\\2", boost::format_sed | boost::format_no_copy);

   //if we found an ip or host
   if(host.length() > 0)
      //transform it into an ip address and put it back into the original string
      return boost::regex_replace(zmqHost, expression, "\\1" + host_to_ip(host) + "\\3", boost::format_sed | boost::format_no_copy);
   else
      return zmqHost;
}

} // anonymous namespace

namespace dqueue {
namespace util {

string make_uuid() {
  char str[37]; // NOTE: uuids are guaranteed by the interface to only be this long
  uuid_t uuid;
  
  uuid_generate(uuid);
  uuid_unparse(uuid, str);
  
  return std::string(str);
}

} // namespace util

namespace conf {

broker::broker(const pt::ptree &config) {
  in_req = parse_zmq_host(config.get<string>("in_req"));
  in_sub = parse_zmq_host(config.get<string>("in_sub"));
  out_req = parse_zmq_host(config.get<string>("out_req"));
  out_sub = parse_zmq_host(config.get<string>("out_sub"));
  monitor = parse_zmq_host(config.get<string>("monitor"));
  in_identity = config.get_optional<string>("in_identity");
  out_identity = config.get_optional<string>("out_identity");
}

common::common(const pt::ptree &config) {
  list<string> broker_names = parse_list(config.get<string>("zmq.broker_names", ""));

  for (list<string>::iterator itr = broker_names.begin();
       itr != broker_names.end(); ++itr) {
    broker c(config.get_child(*itr));
    brokers.insert(make_pair(*itr, c));
  }
}

list<string> 
common::all_in_req() const {
  return brokers_get(*this, &broker::in_req);
}

list<string> 
common::all_in_sub() const {
  return brokers_get(*this, &broker::in_sub);
}

list<string> 
common::all_out_req() const {
  return brokers_get(*this, &broker::out_req);
}

list<string> 
common::all_out_sub() const {
  return brokers_get(*this, &broker::out_sub);
}

} // namespace conf
} // namespace dqueue
