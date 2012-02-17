/*------------------------------------------------------------------------------
 *
 * Program to submit tile render requests directly to the queue.
 *
 *  Author: matt.amos@mapquest.com
 *  Author: kevin.kreiser@mapquest.com
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

#include "version.hpp"
#include "tile_protocol.hpp"
#include "logging/logger.hpp"
#include "dqueue/distributed_queue.hpp"
#include "tile_path_grammar.hpp"

#include <boost/optional.hpp>
#include <boost/format.hpp>
#include <boost/noncopyable.hpp>
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>
#include <boost/spirit/include/qi.hpp>

#include <iostream>
#include <fstream>

using rendermq::tile_protocol;
using rendermq::cmdRenderBulk;
using boost::optional;
using boost::shared_ptr;
using std::string;
using std::ifstream;

namespace po = boost::program_options;
namespace bt = boost::posix_time;
namespace qi = boost::spirit::qi;

// interface to grab a bunch of tiles from somewhere. this 
// might be fed in from stdin, read from a file or a database
// or basically wherever.
struct bunch_of_tiles 
   : public boost::noncopyable
{
   virtual ~bunch_of_tiles() {}

   // gets the next tile from the bunch, or none if there are
   // no tiles left to do.
   virtual optional<tile_protocol> next() = 0;
};

void oops(const tile_protocol &tile)
{
   LOG_ERROR(boost::format("Not supposed to receive anything back "
                           "from broker, but got %1%!") 
             % tile);
}

void runner_poll(dqueue::runner &runner, long timeout)
{
   zmq::pollitem_t items [] = {
      { NULL, 0, ZMQ_POLLIN, 0 },
      { NULL, 0, ZMQ_POLLIN, 0 },
   };
   
   // for the moment assume there's only one pollitem for the distributed queue
   assert(runner.num_pollitems() == 2);
   runner.fill_pollitems(&items[0]);
   
   // poll
   try {
      zmq::poll(&items[0], 2, timeout);
   } catch (const zmq::error_t &) {
      // ignore and loop...
      return;
   }
   
   runner.handle_pollitems(&items[0]);
}

void settle(dqueue::runner &runner)
{
   LOG_INFO("Waiting for queue to settle");
   while (runner.queue_length() >= 1000000)
   {
      runner_poll(runner, 1000);
   }
}

void enqueue_tiles(shared_ptr<bunch_of_tiles> tiles,
                   const string &dqueue_config,
                   size_t short_queue_length,
                   long pause_time,
                   zmq::context_t &ctx)
{
   dqueue::runner runner(dqueue_config, ctx);
   bool queue_short = false;

   LOG_INFO("Setting up session for submitting jobs...");

   // setup the queue runner
   runner.default_handler(
      dqueue::runner::handler_function_t(&oops));

   // settle the queue runner - give it some time to contact
   // brokers, etc...
   settle(runner);

   LOG_INFO("Ready to start submitting jobs.");

   // while there's jobs to be done, and the queue isn't too
   // large, stick 'em in the queue.
   while (true)
   {
      //short circuit if we have no tiles left
      optional<tile_protocol> mtile = tiles->next();
      if (!mtile)
      {
         break;
      }

      // loop while the queue is long, waiting for an opportunity
      // to insert this tile...
      while (!queue_short)
      {
         // wait until something happens
         runner_poll(runner, -1);
         queue_short = runner.queue_length() < short_queue_length;
      }

      //submit this tile
      tile_protocol tile(mtile.get());

      // clip to metatile
      tile.x &= ~0x7;
      tile.y &= ~0x7;
      // ensure there is no response expected
      tile.id = -1;
      // set to lowest priority
      tile.status = cmdRenderBulk;

      // submit the tile
      try
      {
         runner.put_job(tile);
      }
      catch (const dqueue::broker_error &e)
      {
         LOG_ERROR(boost::format("Queue error sending job %1% to queue: %2%.") % tile % e.what());
      }
      catch (const std::exception &e)
      {
         LOG_ERROR(boost::format("Runtime error sending job %1% to queue: %2%.") % tile % e.what());
      }
      catch ( ... )
      {
         LOG_ERROR(boost::format("Unknown error sending job %1% to queue.") % tile);
      }

      // do a short poll, just so that the queue length keeps
      // getting updated.
      bt::ptime wait_until = bt::microsec_clock::local_time() + bt::milliseconds(pause_time / 1000);
      while (bt::microsec_clock::local_time() < wait_until)
      {
         runner_poll(runner, pause_time);
         queue_short = runner.queue_length() < short_queue_length;
      }
   }
}

struct read_from_iostream
   : public bunch_of_tiles
{
   read_from_iostream(std::istream &in) 
      : m_in(in), m_line_count(0)
   {
   }

   ~read_from_iostream() {}

   optional<tile_protocol> next() 
   {
      if (m_in.eof() || m_in.bad())
      {
         return optional<tile_protocol>();
      }
      else
      {
         char line[1024];
         m_in.getline(line, (sizeof line) - 1);
         m_line_count += 1;

         if (m_in.gcount() == (sizeof line) - 1) 
         {
            // line read probably too long. stop processing here to
            // avoid injecting garbage into the queue.
            LOG_ERROR(boost::format("Found line %1% is longer than %2% characters, "
                                    "which is longer than expected for a tile path. "
                                    "Assuming this is a mistake and exiting now.")
                      % m_line_count % ((sizeof line) - 1));
            return optional<tile_protocol>();
         }

         const string sline(line);

         tile_protocol tile;
         bool result = qi::parse(sline.begin(), sline.end(), m_grammar, tile);

         if (result)
         {
            return optional<tile_protocol>(tile);
         }
         else
         {
            LOG_ERROR(boost::format("Line %1% does not match the expected tile path "
                                    "format, e.g: /map/0/0/0.jpg.")
                      % m_line_count);
            return optional<tile_protocol>();
         }
      }
   }   

private:
   std::istream &m_in;
   size_t m_line_count;
   rendermq::tile_path_grammar<std::string::const_iterator> m_grammar;
};

int main(int argc, char *argv[])
{
   std::string dqueue_config = "dqueue.conf";
   size_t short_queue_length = 2000;
   long pause_time = 5000;

   po::options_description cmdline_options("Tile submitter\n"
                                           "Version: " VERSION "\n"
                                           "\n"
                                           "Generic options:");
   cmdline_options.add_options()
      ("help,h", "Print this help message")
      ("queue-config,C", 
       po::value<std::string>(&dqueue_config)->default_value("dqueue.conf"), 
       "Path to the dqueue configuration file.")
      ("short-length,l", 
       po::value<size_t>(&short_queue_length)->default_value(2000),
       "How short does the queue have to be to continue submitting jobs.")
      ("pause-time,t", 
       po::value<long>(&pause_time)->default_value(5000),
       "How long to wait between submitting jobs in microseconds. Use this "
       "to alter the steady-state rate at which jobs are put in the queue.")
      ("input-file,f", po::value<std::string>(),
       "Required input file, use '-' to read from stdin.")
      ;

   po::variables_map vm;
   try
   {
      po::store(po::command_line_parser(argc, argv)
                .options(cmdline_options)
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

   if (!vm.count("queue-config") || !vm.count("short-length"))
   {
      std::cerr << cmdline_options << "\n";
      return EXIT_FAILURE;
   }
   
   shared_ptr<bunch_of_tiles> tiles;
   ifstream in;
   if (vm.count("input-file") == 0) 
   {
      std::cerr << "No input-file option provided.\n";
      std::cerr << cmdline_options << "\n";
      return EXIT_FAILURE;
   }

   string file_name(vm["input-file"].as<std::string>());
   if (file_name != "-")
   {
      in.open(file_name.c_str(), ifstream::in);

      if (in.fail())
      {
         LOG_ERROR(boost::format("Cannot open file `%1%' for reading.") % file_name);
         return EXIT_FAILURE;
      }

      tiles = shared_ptr<bunch_of_tiles>(new read_from_iostream(in));
   }
   else
   {
      tiles = shared_ptr<bunch_of_tiles>(new read_from_iostream(std::cin));
   }

   zmq::context_t ctx(1);

   enqueue_tiles(tiles, dqueue_config, short_queue_length, pause_time, ctx);
   
   return 0;
}
