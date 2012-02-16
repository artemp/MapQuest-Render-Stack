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

#include <zmq.hpp>
#include "zstream.hpp"
#include "dqueue/distributed_queue_config.hpp"
#include "config.hpp"
#include "zmq_utils.hpp"

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <map>
#include <queue>
#include <iostream>
#include <sstream>
#include <vector>

using std::string;
using std::vector;
using std::list;
using std::map;
using std::string;
using std::ostringstream;
using boost::format;
namespace manip = zstream::manip;
namespace pt = boost::property_tree;
namespace po = boost::program_options;
namespace bt = boost::posix_time;

struct broker_info
{
   broker_info()
      : last_seen(), queue_size(0)
   {
   }

   bt::ptime last_seen;
   uint64_t queue_size;
};

int main (int argc, char** argv) 
{
   typedef map<string, dqueue::conf::broker>::value_type broker_conf_type;
   string dqueue_config;
   unsigned int screen_update_time = 1;

   po::options_description cmdline_options("Broker control / utility\n"
                                           "Version: " VERSION "\n"
                                           "\n"
                                           "Options:");
   cmdline_options.add_options()
      ("help,h", "Print this help message.")
      ("command,c", po::value<string>(), "Command to send to the broker.")
      ("config-file,f", po::value<string>(&dqueue_config)->default_value("dqueue.conf"),
       "Path to distributed queue configuration file.")
      ("monitor,m", "Monitor the brokers and print the results.")
      ("update-interval,i", po::value<unsigned int>(&screen_update_time)->default_value(5),
       "Interval between screen updates in monitor mode.")
      ("quiet,q", "Only print out the time and total queue size in monitor mode.")
      ("single,s", "Only print out one update in monitor mode.")
      ("broker-names", po::value<vector<string> >(), "Broker names to send commands to "
       "(Only needed if -c used).")
      ;

   po::positional_options_description p_desc;
   p_desc.add("broker-names", -1);

   po::variables_map vm;
   try 
   {
      po::store(po::command_line_parser(argc, argv)
                .options(cmdline_options)
                .positional(p_desc)
                .run(), 
                vm);
      po::notify(vm);
   }
   catch (const std::exception &e)
   {
      std::cerr << e.what() << "\n";
      return EXIT_FAILURE;
   }

   if (vm.count("help"))
   {
      std::cerr << cmdline_options << "\n";
      return EXIT_FAILURE;
   }

   // read config file
   pt::ptree config;
   try 
   {
      pt::read_ini(dqueue_config, config);
   }
   catch (const std::exception &e)
   {
      std::cerr << "ERROR: " << e.what() << "\n";
      return EXIT_FAILURE;
   }
   dqueue::conf::common dconf(config);

   // check the loaded version of 0MQ library
   rendermq::zmq_check_version_ok();
   
   zmq::context_t context(1);

   if (vm.count("command"))
   {
      string message = vm["command"].as<string>();
      vector<string> brokers;
      if (vm.count("broker-names") > 0)
      {
         brokers = vm["broker-names"].as<vector<string> >();
      }
      if (brokers.empty())
      {
         BOOST_FOREACH(broker_conf_type b, dconf.brokers)
         {
            brokers.push_back(b.first);
         }
      }
      BOOST_FOREACH(string broker_name, brokers)
      {
         map<string,dqueue::conf::broker>::iterator self = dconf.brokers.find(broker_name);
         if (self == dconf.brokers.end()) {
            std::cerr << "Broker name `" << broker_name << "' isn't present in config file." << std::endl;
            return EXIT_FAILURE;
         }

         zstream::socket::req cmd(context);
         cmd.connect(self->second.monitor); 
         string reply;
  
         cmd << message;
         cmd >> reply;

         std::cout << "Broker[" << broker_name << "] replied: `" << reply << "'" << std::endl;
      }
   }

   if (vm.count("monitor"))
   {
      typedef map<string, broker_info>::value_type broker_info_type;
      zstream::socket::sub sub(context);
      map<string, broker_info> broker_infos;
      bool quiet = vm.count("quiet") > 0;
      bool single = vm.count("single");

      BOOST_FOREACH(broker_conf_type b, dconf.brokers)
      {
         if (!quiet) { std::cout << " SUB " << b.second.in_sub << "\n"; }
         sub.connect(b.second.in_sub);
      }

      bt::ptime last_update = bt::microsec_clock::local_time();
      do
      {
         zmq::pollitem_t items [] = { { sub.socket(), 0, ZMQ_POLLIN, 0 } };
         try 
         {
            zmq::poll(&items[0], 1, 50000);
         } 
         catch (const zmq::error_t &e) 
         {
            std::cerr << "ERROR during poll: " << e.what() << "\n";
            return EXIT_FAILURE;
         }

         if (items[0].revents & ZMQ_POLLIN)
         {
            string broker_id;
            uint64_t qsize;

            sub >> broker_id >> qsize;

            broker_infos[broker_id].last_seen = bt::microsec_clock::local_time();
            broker_infos[broker_id].queue_size = qsize;
         }

         bt::ptime now = bt::microsec_clock::local_time();
         if ((now - last_update) >= bt::seconds(screen_update_time)) 
         {
            if (quiet) { std::cout << now; } else { std::cout << " == " << now << " ==\n"; }
            uint64_t total = 0;
            BOOST_FOREACH(broker_info_type x, broker_infos) 
            {
               if (!quiet)
               {
                  std::cout << x.first << "\t" << x.second.queue_size << "\t" 
                            << now - x.second.last_seen << "\n";
               }
               total += x.second.queue_size;
            }
            if (quiet) { 
               std::cout << " qsize= " << total; 
            } else { 
               std::cout << "* Total queue size = " << total << "\n"; 
            }
            std::cout << std::endl;
            last_update = now;
            if(single)
               break;
         }
      } while(true);
   }

   return EXIT_SUCCESS;
}

