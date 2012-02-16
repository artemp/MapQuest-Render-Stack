/*------------------------------------------------------------------------------
 *
 * Implementation of the expiry service, a redundant (2x) server
 * to handle queries as to whether a tile is expired or not.
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

#include "../zstream.hpp"
#include "../zstream_pbuf.hpp"
#include "../config.hpp"
#include "expiry_server.hpp"
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

using std::string;
namespace po = boost::program_options;
namespace pt = boost::property_tree;

int main(int argc, char *argv[]) 
{
   string config_file;

   po::options_description cmdline_options("Expiry Server\n"
                                           "Version: " VERSION "\n"
                                           "\n"
                                           "Options:");
   cmdline_options.add_options()
      ("help,h", "Print this message.")
      ("primary,p", "This server is the primary.")
      ("backup,b", "This server is the backup.")
      ("config,c", po::value<string>(&config_file)->default_value("expiry_server.conf"),
       "Path to configuration file.")
      ;

   po::variables_map vm;
   try 
   {
      po::store(po::command_line_parser(argc, argv)
                .options(cmdline_options)
                .run(), vm);
      po::notify(vm);
   }
   catch (const std::exception &e)
   {
      std::cerr << e.what() << "\n";
      return EXIT_FAILURE;
   }

   if (vm.count("help")) 
   {
      std::cout << cmdline_options << "\n";
      return EXIT_SUCCESS;
   }

   pt::ptree conf;
   try 
   {
      pt::read_ini(config_file, conf); 
   }
   catch (pt::ptree_error & err)
   {
      std::cerr << "ERROR while parsing: " << config_file << std::endl;
      std::cerr << err.what() << std::endl;
      return EXIT_FAILURE;
   }

   if ((vm.count("primary") > 0) == (vm.count("backup") > 0))
   {
      std::cerr << "Server must be either primary, or backup.\n";
      return EXIT_FAILURE;
   }

   rendermq::expiry_server::state_t state = 
      (vm.count("primary") > 0) ?
      rendermq::expiry_server::state_primary : 
      rendermq::expiry_server::state_backup;

   zmq::context_t context(1);

   rendermq::expiry_server server(context, state, conf);

   // run the server
   server();

   return EXIT_SUCCESS;
}
