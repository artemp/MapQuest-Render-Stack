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

#include "tile_handler.hpp"
#include "config.hpp"
#include "zmq_utils.hpp"
#include "logging/logger.hpp"

#include <string>
#include <stdexcept>
#include <map>
#include <list>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

// for gethostname
#include <unistd.h>

#define DEFAULT_QUEUE_THRESHOLD_STALE (100)
#define DEFAULT_QUEUE_THRESHOLD_SATISFY (500)
#define DEFAULT_QUEUE_THRESHOLD_MAX (1000)
#define DEFAULT_IO_MAX_CONCURRENCY (64)

namespace po = boost::program_options;
namespace pt = boost::property_tree;
using std::map;
using std::string;
using std::list;

// maximum length of a hostname from gethostname().
#define HOSTNAME_MAX (1024)

namespace {

map<string, list<string> > dirty_list_from_conf(const pt::ptree &conf)
{
   map<string, list<string> > deps;

   boost::optional<const pt::ptree &> maybe_sect = conf.get_child_optional("dirty");
   if (maybe_sect)
   {
      const pt::ptree &sect = maybe_sect.get();
      BOOST_FOREACH(const pt::ptree::value_type &entry, sect)
      {
         string style_name = entry.first;
         list<string> dep_names;
         string dep_list = sect.get<string>(style_name);
         boost::split(dep_names, dep_list, boost::is_any_of(", "), boost::token_compress_on);
         deps.insert(make_pair(style_name, dep_names));
      }
   }
   
   return deps;
}

// turn an instance command line option into a set of strings
// to be tried as the section headers.
list<string> host_sections(const string &instance)
{
   using std::ostringstream;
   using boost::iterator_range;

   list<string> sections;

   // try the original string, as given.
   sections.push_back(instance);

   // try the short and full hostnames prepended to the original
   // string.
   char hostname_c[HOSTNAME_MAX];
   if (gethostname(hostname_c, HOSTNAME_MAX) != 0) {
      std::cerr << "Error while getting hostname: " << strerror(errno) << "\n";
      exit(EXIT_FAILURE);
   }
   string hostname(hostname_c);

   // make the key for use in the config file from the hostname plus the
   // instance name (likely just a numeric index).

   // first replace dashes with underscores in the hostname.
   boost::algorithm::replace_all(hostname, "-", "_");

   { // short host name
      iterator_range<string::iterator> result = boost::algorithm::find_first(hostname, ".");
      string short_hostname(hostname.begin(), result.begin());
      
      ostringstream ostr;
      ostr << short_hostname << "_" << instance;
      sections.push_back(ostr.str());
   }

   { // full host name
      ostringstream ostr;
      ostr << hostname << "_" << instance;
      sections.push_back(ostr.str());
   }

   return sections;
}

}

int main( int argc, char** argv)
{
   string config_file, dqueue_config;
    
   po::options_description cmdline_options("Mongrel2 Tile Handler\n" 
                                           "Version: " VERSION "\n"
                                           "\n"
                                           "Generic Options:");
   cmdline_options.add_options()
      ("help,h","print this message")
      ("uuid,u",po::value<string>(),"Unique identifier for the handler (overrides config value)")
      ("instance,i",po::value<string>(),"Instance name or number. The config file will be "
       "searched for a section with this name, or this name prepended by the short hostname or "
       "long hostname.")
      ("config,c", po::value<string>(&config_file)->default_value("tile_handler.conf"),
       "Path to configuration file.")
      ("queue-config,C", po::value<string>(&dqueue_config)->default_value("dqueue.conf"),
       "Path to distributed queue configuration file.")
      ("logging-config,l", po::value<string>(), "Location of the logging configuration file.")
      ;
   po::variables_map vm;
   try 
   {
      po::store(po::command_line_parser(argc,argv).options(cmdline_options).run(),vm);
      po::notify(vm);

      if (vm.count("help"))
      {
         std::cerr << cmdline_options << "\n";
         return EXIT_SUCCESS;
      }
        
   }
   catch (std::exception & e)
   {
      std::cerr << e.what() << "\n";
      return EXIT_FAILURE;
   }
    
   if (!vm.count("config"))
   {
      std::cerr << cmdline_options << "\n";
      return EXIT_FAILURE;
   }

   // check the loaded version of 0MQ library
   rendermq::zmq_check_version_ok();
   
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

   // used by the handler to determine if a style should be passed
   // to the worker or be considered a 404.
   rendermq::style_rules style_rules(conf);

   // get the uuid from conf or command line
   string uuid;
   if (vm.count("uuid")) {
      uuid = vm["uuid"].as<string>();

   } else if (vm.count("instance")) {
      string instance = vm["instance"].as<string>();
      list<string> sections = host_sections(instance);
      string section_name;
      boost::optional<pt::ptree &> host_config;

      // find the first item in the list which matches an available
      // section name.
      BOOST_FOREACH(const string &section, sections)
      {
         host_config = conf.get_child_optional(section);
         
         if (host_config)
         {
            section_name = section;
            break;
         }
      }

      // didn't find a section with any of those names, so report an
      // error.
      if(!host_config)
      {
         std::cerr << "Could not find any sections matching (";
         BOOST_FOREACH(const string &section, sections)
         {
            std::cerr << "\"" << section << "\" ";
         }
         std::cerr << ") in the config, and no UUID specified on the command line.\n";
         exit(EXIT_FAILURE);
      }

      boost::optional<string> maybe_uuid = host_config->get_optional<string>("uuid");
      if (!maybe_uuid) {
         std::cerr << "UUID for host key `" << section_name << "' not specified in "
                   << "the host section of the handler config file.\n";
         exit(EXIT_FAILURE);
      }

      uuid = *maybe_uuid;

   } else {
      std::cerr << "Must specify either the instance name (or number) for the config "
                << "file, or the UUID on the command line.\n";
      exit(EXIT_FAILURE);
   }

   // try to load the logging configuration
   if (vm.count("logging-config")) 
   {
      string logging_conf_file = vm["logging_config"].as<string>();
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

   // get the dependencies between styles for the purposes of 
   // expiry-chaining.
   map<string, list<string> > dirty_deps = dirty_list_from_conf(conf);

   rendermq::tile_handler handler(
      uuid,
      conf.get<string>("mongrel2.in_endpoint","ipc:///tmp/mongrel_send"),
      conf.get<string>("mongrel2.out_endpoint","ipc:///tmp/mongrel_recv"),
      conf.get<std::time_t>("mongrel2.max_age",60*60*24),
      conf.get<size_t>("mongrel2.queue_threshold_stale", DEFAULT_QUEUE_THRESHOLD_STALE),
      conf.get<size_t>("mongrel2.queue_threshold_satisfy", DEFAULT_QUEUE_THRESHOLD_SATISFY),
      conf.get<size_t>("mongrel2.queue_threshold_max", DEFAULT_QUEUE_THRESHOLD_MAX),                    
      conf.get<bool>("mongrel2.stale_render_background", false),
      conf.get<size_t>("mongrel2.max_io_concurrency", DEFAULT_IO_MAX_CONCURRENCY),
      dqueue_config, conf.get_child("tiles"), style_rules, dirty_deps);

   handler();
    
   return EXIT_SUCCESS;
}
