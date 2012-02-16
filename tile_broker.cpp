/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
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

#include <zmq.hpp>
#include "zmq_utils.hpp"
#include "tile_protocol.hpp"
#include "zstream.hpp"
#include "zstream_pbuf.hpp"
#include "dqueue/distributed_queue_config.hpp"
#include "tile_broker_impl.hpp"
#include "config.hpp"
#include "logging/logger.hpp"

#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <boost/array.hpp>
#include <boost/tokenizer.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/program_options.hpp>

#include <map>
#include <queue>
#include <iostream>
#include <sstream>

using std::string;
using std::list;
using std::map;
using std::string;
using std::ostringstream;
using boost::format;
namespace manip = zstream::manip;
namespace pt = boost::property_tree;
namespace po = boost::program_options;

int main (int argc, char** argv)
{
   using namespace rendermq;

   std::string config_file, broker_name;

   po::options_description cmdline_options("Tile broker\n"
                                           "Version: " VERSION "\n"
                                           "\n"
                                           "Generic options:");
   cmdline_options.add_options()
      ("help,h", "Print help message")
      ("logging-config,l", po::value<std::string>(), "Location of the logging configuration file.")
      ("queue-config,C", po::value<std::string>(&config_file), "Path to the dqueue configuration file.")
      ("name,n", po::value<std::string>(&broker_name), "The broker name (as used in the dqueue config).")
      ;

   po::positional_options_description pos;
   pos.add("queue-config", 1);
   pos.add("name", 1);

   po::variables_map vm;

   try 
   {
      po::store(po::command_line_parser(argc, argv)
                .options(cmdline_options)
                .positional(pos)
                .run(), vm);
      po::notify(vm);

      if (vm.count("help")) 
      {
         std::cerr << cmdline_options << "\n";
         return EXIT_SUCCESS;
      }
   }
   catch (std::exception &e) 
   {
      std::cerr << e.what() << "\n";
      return EXIT_FAILURE;
   }

   if (!vm.count("name") || !vm.count("queue-config"))
   {
      std::cerr << cmdline_options << "\n";
   }

   // read config file
   pt::ptree config;
   try 
   {
      pt::read_ini(config_file, config);
   }
   catch (pt::ptree_error & err)
   {
      std::cerr << "ERROR while parsing: " << config_file << std::endl;
      std::cerr << err.what() << std::endl;
      return EXIT_FAILURE;
   }
   
   // check the loaded version of 0MQ library
   zmq_check_version_ok();
   
   // try to load the logging configuration
   if (vm.count("logging-config")) 
   {
      std::string logging_conf_file = vm["logging_config"].as<std::string>();
      try
      {
         pt::ptree logging_config;
         pt::read_ini(logging_conf_file, logging_config);
         rendermq::log::configure(logging_config);
      }
      catch (const std::exception &e)
      {
         std::cerr << "Error while setting up logging from: " << logging_conf_file << "\n";
         exit(EXIT_FAILURE);
      }
   }

   // main entry point for broker:
   try {
      // set up the broker
      zmq::context_t context(1);
      broker_impl impl(config, argv[2], context);
      
      // run the broker
      impl();
      
   } catch (const std::exception &e) {
      LOG_ERROR(e.what());
      return EXIT_FAILURE;
      
   } catch (...) {
      LOG_ERROR("UNKNOWN ERROR");
      return EXIT_FAILURE;
   }
   
   // give some time for sockets to drain.
   sleep (1); 
    
   return EXIT_SUCCESS;
}

