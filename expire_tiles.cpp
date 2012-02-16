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

#include "dqueue/distributed_queue.hpp"
#include "dqueue/distributed_queue_config.hpp"
#include "storage/tile_storage.hpp"
#include "storage/meta_tile.hpp"
#include "storage/lts_storage.hpp"
#include "tile_utils.hpp"
#include "zmq_utils.hpp"
#include "config.hpp"

#include <boost/format.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/tokenizer.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/unordered_set.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/microsec_time_clock.hpp>

#include <map>
#include <set>
#include <queue>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <limits>

using std::string;
using std::list;
using std::vector;
using std::map;
using std::set;
using std::cerr;
using std::cout;
using std::endl;
using std::flush;
using std::string;
using std::ostringstream;
using std::ifstream;
using std::runtime_error;
using std::numeric_limits;
using boost::optional;
using boost::shared_ptr;
using boost::scoped_ptr;
using boost::unordered_set;
using rendermq::protoFmt;
namespace pt = boost::property_tree;
namespace po = boost::program_options;
namespace bt = boost::posix_time;

// how many requests to batch into a single connection to a single
// host. why 9024? well, it's OVER 9000!!!! and a multiple of 64,
// the Magic Number. actually, it turns out that OVER 9000!!! is a
// bit too much, so lowering to something that should be around
// 4 sec of work.
#define LTS_QUEUE_BATCH_SIZE (1024)

// global verbosity flag
bool g_verbose;

// global dry-run flag
bool g_perform_actions;

// took out re-render thing - we now have a tile_submitter anyway
// for those re-rendering tasks...

vector<string> parse_list(const string &str) {
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
  boost::char_separator<char> sep(", \t");
  tokenizer_t tok(str, sep);

  vector<string> l;
  std::copy(tok.begin(), tok.end(), std::back_inserter(l));
  return l;
}

class storage_expire
  : public boost::noncopyable 
{
public:
   typedef map<pair<string, bool>,list<string> > lts_queue_t;

   storage_expire(
      const string &file_name,
      optional<int> min_z,
      optional<int> max_z,
      size_t total_tiles) 
   {
      pt::ptree config;
      pt::read_ini(file_name, config);
      pt::ptree params = config.get_child("storage");

      // construct the storage and special-case it for LTS, as we have
      // to handle things slightly differently here. basically, since 
      // each metatile is spread out over several hosts then accessing
      // things in metatile order is particularly inefficient. instead,
      // we want to batch as many LTS expires as possible to a single
      // host and do those to take advantage of connection pipelining.
      rendermq::tile_storage *ptr = rendermq::get_tile_storage(params);
      rendermq::lts_storage *lts_ptr = dynamic_cast<rendermq::lts_storage *>(ptr);
      if (lts_ptr == 0) {
         m_storage.reset(ptr);
      } else {
         m_lts_storage.reset(lts_ptr);
      }

      m_min_z = min_z ? min_z.get() : numeric_limits<int>::min();
      m_max_z = max_z ? max_z.get() : numeric_limits<int>::max();

      m_total_tiles = total_tiles;
      m_last_percent = 0;
   }

   bool drain()
   {
      // LTS special case needs to be drained, because we
      // batch up operations and there might still be some
      // left over.
      if (!m_storage)
      {
         return lts_special_drain();
      }
      return true;
   }
   
   bool operator()(const dqueue::job_t &job) 
   {
      // for generic storage
      if (m_storage) 
      {
         return generic_expire(job);
      } 
      else
      {
         return lts_special_expire(job);
      }
   }

   bool generic_expire(const dqueue::job_t &job)
   {
      shared_ptr<rendermq::tile_storage::handle> handle = m_storage->get(job);
      // only want to continue the chain and re-render if the tile was present
      // in storage in the first place...
      bool is_clean = handle->exists() && !handle->expired();
      if (is_clean && (job.z >= m_min_z) && (job.z <= m_max_z))
      {
         if (g_verbose) { std::cout << "Expiring " << job << "\n"; }
         m_storage->expire(job);
      }
      return is_clean;
   }

   bool lts_special_expire(const dqueue::job_t &job)
   {
      // unpack each tile to get the host which handles that tile and
      // the URL for it and batch these up per-host.
      lts_queue_urls(job);
      
      // when the batch gets large enough, execute it.
      BOOST_FOREACH(lts_queue_t::value_type &itr, m_lts_request_queue)
      {
         if (itr.second.size() > LTS_QUEUE_BATCH_SIZE)
         {
            lts_perform_urls(itr);

            float percent = float(m_num_complete * 100) / m_total_tiles;
            if (int(percent) > int(m_last_percent)) 
            {
               LOG_FINER(boost::format("Complete: %1% percent.") % percent);
               m_last_percent = percent;
            }
         }
      }

      return true;
   }

   bool lts_special_drain()
   {
      LOG_DEBUG("Draining...");

      BOOST_FOREACH(lts_queue_t::value_type &itr, m_lts_request_queue)
      {
         if (!itr.second.empty())
         {
            lts_perform_urls(itr);
         }
      }

      LOG_DEBUG("Drained.");
      
      return true;
   }

   void lts_queue_urls(const dqueue::job_t &job) 
   {
      // if LTS is going to use expiry information on a per-metatile 
      // basis, then we only need to send one path per host and it
      // doesn't matter whether it's the primary or not...
      typedef map<string, pair<string, bool> > uniq_host_t;
      uniq_host_t unique_hosts;

      lts_fill_hosts(unique_hosts, job, true);
      lts_fill_hosts(unique_hosts, job, false);

      BOOST_FOREACH(uniq_host_t::value_type &row, unique_hosts)
      {
         pair<string, bool> host = make_pair(row.first, row.second.second);
         const string &path = row.second.first;

         lts_queue_t::iterator itr = m_lts_request_queue.find(host);
         if (itr == m_lts_request_queue.end()) 
         {
            pair<lts_queue_t::iterator, bool> result 
               = m_lts_request_queue.insert(make_pair(host, list<string>()));
            itr = result.first;
         }
         itr->second.push_back(path);
      }
   }

   void lts_fill_hosts(map<string, pair<string, bool> > &hosts,
                       const dqueue::job_t &job,
                       bool is_primary)
   {
      const vector<string> urls = m_lts_storage->make_get_urls(job, is_primary);

      BOOST_FOREACH(const string &url, urls)
      {
         // find path separator, skipping "http://"
         size_t path_starts = url.find_first_of('/', 7);
         if (path_starts != string::npos) 
         {
            // host has to contain "primaryness" information otherwise the
            // later sections won't know whether to send the primary or
            // replica headers.
            string host = url.substr(0, path_starts);

            if (hosts.find(host) == hosts.end())
            {
               string path = url.substr(path_starts);
               hosts.insert(make_pair(host, make_pair(path, is_primary)));
            }
         }
      }
   }

   bool lts_perform_urls(lts_queue_t::value_type &itr)
   {
      vector<string> urls;
      const pair<string, bool> &host = itr.first;
      bt::ptime t_start = bt::microsec_clock::local_time();

      try 
      {
         http::curl_ptr connection = http::createPersistentConnection();
         const http::headers_t headers = m_lts_storage->expiry_headers(host.second);

         BOOST_FOREACH(const string &path, itr.second)
         {
            string url = host.first;
            url.append(path);
            // todo: timeout
            http::get(url, connection, headers, false, 300L);
         }
      }
      catch (const std::runtime_error &e)
      {
         LOG_ERROR(boost::format("Runtime error while expiring LTS tile batch: %1%") % e.what());

         // note, on error, maybe leave these urls in?
         m_num_complete += itr.second.size();
         itr.second.clear();

         return false;
      }

      bt::ptime t_end = bt::microsec_clock::local_time();
      float rate = itr.second.size() / ((t_end - t_start).total_microseconds() / 1000000.0);
      LOG_FINER(boost::format("Expired %1% tiles on %2% (%3%) at %4% per sec.") 
                % itr.second.size() % host.first % host.second % rate);

      m_num_complete += itr.second.size();
      itr.second.clear();
      return true;
   }

private:
   scoped_ptr<rendermq::tile_storage> m_storage;
   scoped_ptr<rendermq::lts_storage> m_lts_storage;
   lts_queue_t m_lts_request_queue;
   int m_num_complete;
   int m_min_z, m_max_z;
   size_t m_total_tiles;
   float m_last_percent;
};

struct tile_parser 
{
   enum token_order 
   {
      order_x_y_z,
      order_z_x_y
   };

   typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;

   tile_parser(const string &delimiter, token_order tkn_order) 
      : m_delimiter(delimiter), m_token_order(tkn_order) 
   {
   }

   rendermq::tile_protocol operator()(const string &l) 
   {
#define ASSERT_LINE_NOT_FINISHED if (itr == tok.end()) { throw runtime_error("Malformatted line."); }
#define ASSERT_LINE_IS_FINISHED if (itr != tok.end()) { throw runtime_error("Malformatted line."); }

      boost::char_separator<char> sep(m_delimiter.c_str());
      const string line(l);
      tokenizer_t tok(line, sep);
      
      rendermq::tile_protocol tile;
      
      tokenizer_t::iterator itr = tok.begin();
      if (m_token_order == order_z_x_y) {
         tile.z = boost::lexical_cast<int>(*itr); ++itr;
         ASSERT_LINE_NOT_FINISHED;
      }

      tile.x = boost::lexical_cast<int>(*itr); ++itr;
      ASSERT_LINE_NOT_FINISHED;
      tile.y = boost::lexical_cast<int>(*itr); ++itr;

      if (m_token_order != order_z_x_y) {
         ASSERT_LINE_NOT_FINISHED;
         tile.z = boost::lexical_cast<int>(*itr); ++itr;
      }

      ASSERT_LINE_IS_FINISHED;
      
      return tile;

#undef ASSERT_LINE_NOT_FINISHED
#undef ASSERT_LINE_IS_FINISHED
   }

   string m_delimiter;
   token_order m_token_order;
};

struct xyz 
   : public boost::less_than_comparable<xyz>,
     public boost::equality_comparable<xyz>
{
   xyz(int x, int y, int z) : m_x(x), m_y(y), m_z(z) {}

   bool operator==(const xyz &other) const 
   {
      return ((m_z == other.m_z) &&
              (m_x == other.m_x) &&
              (m_y == other.m_y));
   }

   int m_x, m_y, m_z;
};

std::ostream &operator<<(std::ostream &out, const xyz &tile)
{
   out << "xyz(" << tile.m_x << "," << tile.m_y << "," << tile.m_z << ")";
   return out;
}

size_t hash_value(const xyz &tile) 
{
   size_t seed = 0;
   boost::hash_combine(seed, tile.m_x);
   boost::hash_combine(seed, tile.m_y);
   boost::hash_combine(seed, tile.m_z);
   return seed;
}

// recursively add child metatiles of the argument tile which aren't 
// already in the set of metatiles to the set, up to some maximum z.
void child_tiles(xyz tile, int max_z, unordered_set<xyz> &tiles) 
{
   {
      xyz meta = tile;
      meta.m_x &= ~(METATILE - 1);
      meta.m_y &= ~(METATILE - 1);
      if (tiles.count(meta) == 0)
      { 
         tiles.insert(meta);
      }
   }
   if (tile.m_z < max_z) 
   {
      // make the child tile.
      xyz child = tile;
      child.m_z += 1;
      child.m_x <<= 1;
      child.m_y <<= 1;

      // recurse on the 4 children of the metatile
      child_tiles(child, max_z, tiles);
      child.m_x += 1;
      child_tiles(child, max_z, tiles);
      child.m_y += 1;
      child_tiles(child, max_z, tiles);
      child.m_x -= 1;
      child_tiles(child, max_z, tiles);
   }
}

// recursively find parent metatiles and, if not in the set, add to the
// set. yeah, this could be a loop - no need for it to be recursive,
// but i liked the symmetry of it.
void parent_tiles(xyz tile, int min_z, unordered_set<xyz> &tiles)
{
   {
      xyz meta = tile;
      meta.m_x &= ~(METATILE - 1);
      meta.m_y &= ~(METATILE - 1);
      if (tiles.count(meta) == 0)
      { 
         tiles.insert(meta);
      }
   }
   if (tile.m_z > min_z) 
   {
      xyz parent = tile;
      parent.m_z -= 1;
      parent.m_x = tile.m_x >> 1;
      parent.m_y = tile.m_y >> 1;

      parent_tiles(parent, min_z, tiles);
   }
}

typedef boost::function<bool (const rendermq::tile_protocol &)> sink_func_t;

void expire_tiles(string config_file,
                  optional<int> min_z, optional<int> max_z,
                  const vector<string> &styles, 
                  const map<string, string> &types,
                  const map<string, vector<protoFmt> > &formats, 
                  const list<xyz> &tile_list) 
{
   rendermq::tile_protocol tile;

   size_t total_tiles = 0;
   BOOST_FOREACH(string style, styles)
   {
      map<string, vector<protoFmt> >::const_iterator fmt_itr = formats.find(style);
      total_tiles += fmt_itr->second.size();
   }
   total_tiles *= tile_list.size();

   storage_expire expire(config_file, min_z, max_z, total_tiles);

   BOOST_FOREACH(xyz tile_xyz, tile_list) 
   {
      tile.status = rendermq::cmdRenderBulk;
      tile.z = tile_xyz.m_z;
      tile.x = tile_xyz.m_x;
      tile.y = tile_xyz.m_y;

      // just a check that no non-metatiles have sneaked through
      // into the list of stuff to do.
      assert((tile.x & (METATILE - 1)) == 0);
      assert((tile.y & (METATILE - 1)) == 0);
      
      BOOST_FOREACH(string style, styles) 
      {
         map<string, string>::const_iterator type_itr = types.find(style);
         map<string, vector<protoFmt> >::const_iterator fmt_itr = formats.find(style);
         if ((type_itr != types.end()) &&
             (fmt_itr != formats.end()))
         {
            tile.style = type_itr->second;
            const vector<protoFmt> &protoFmts = fmt_itr->second;

            BOOST_FOREACH(protoFmt format, protoFmts)
            {
               tile.format = format;
               if (g_verbose) { std::cout << "Processing: " << tile << "\n"; }
               if (g_perform_actions)
               {
                  expire(tile);
               }
            }
         }
      }
   }

   // if we're using the LTS special case then this 
   // needs to happen to ensure that all expiries are
   // executed.
   expire.drain();
}

int main (int argc, char** argv) 
{
   po::options_description desc("Expire Tiles\n" 
                                "Version: " VERSION "\n"
                                "\n"
                                "Options:");
   desc.add_options()
      ("help", "This help message.")
      ("verbose,v", "Output extra information.")
      ("dry-run,n", "Do not actually perform any actions, just go through the motions.")
      ("latlon,l", "Input file is in lat, lon order.")
      ("lonlat,L", "Input file is in lon, lat order.")
      ("tiles,t", "Input file is x, y, z triples.")
//      ("rerender-with", po::value<string>(), "Re-render expired tiles using the given dqueue config file.")
      ("worker-config,w", po::value<string>(), "Worker config to read to get active styles.")
      ("style", po::value<vector<string> >(), "Style names to expire (repeat the argument).")
      ("delim,d", po::value<string>()->default_value(" "), "Input delimiter")
      ("zxy", "Input is in z/x/y order.")
      ("xyz", "Input is in x/y/z order.")
      ("expire-down-to,e", po::value<int>(), "Also expire tiles at lower z which are parents of input tiles down to this minimum.")
      ("expire-up-to,E", po::value<int>(), "Also expire tiles at higer z which are children of input tiles up to this maximum.")
//      ("rerender-down-to,r", po::value<int>(), "Also re-render tiles at lower z which are parents of input tiles down to this minimum.")
//      ("rerender-up-to,R", po::value<int>(), "Also re-render tiles at higer z which are children of input tiles up to this maximum.")
      ("num-threads,p", po::value<size_t>()->default_value(1), "How many threads to use while expiring.")
      ("input-files", po::value<vector<string> >(), "Input file(s).")
      ;

   po::positional_options_description p_desc;
   p_desc.add("input-files", -1);
   
   po::variables_map vm;
   po::store(po::command_line_parser(argc, argv).
             options(desc).positional(p_desc).run(),
             vm);
   po::notify(vm);
   
   if (vm.count("help") || (vm.count("input-files") == 0)) {
      cout << desc << endl;
      return EXIT_SUCCESS;
   }
   
   // check the loaded version of 0MQ library
   rendermq::zmq_check_version_ok();
   
   vector<string> styles;
   pt::ptree config;
   if (vm.count("worker-config")) {
      pt::read_ini(vm["worker-config"].as<string>(), config);
      string strstyles = config.get<string>("worker.styles");
      styles = parse_list(strstyles);
   } else {
      cerr << "You must provide the worker-config parameter." << endl;
      return EXIT_FAILURE;
   }
   // allow override for the styles
   if (vm.count("style") > 0) {
      styles = vm["style"].as<vector<string> >();
   }
   
   //get the formats and tile style for each style
   map<string, vector<protoFmt> > formats;
   map<string, string> types;
   for(vector<string>::const_iterator style = styles.begin(); style != styles.end(); style++)
   {
      //save the coverage type for the style
      //string type = config.get<string>((*style) + ".type");
      types[*style] = *style;
      //get the list of formats for this type
      vector<string> strformats = parse_list(config.get<string>(string("formats.") + *style));
      vector<protoFmt> protoFmts;
      for(vector<string>::const_iterator format = strformats.begin(); format != strformats.end(); format++)
         protoFmts.push_back(rendermq::get_format_for(*format));
      //save the formats for this style
      formats[*style] = protoFmts;
   }

   string delimiter = vm["delim"].as<string>();
   boost::function<rendermq::tile_protocol (const string &)> parser;
   if (vm.count("tiles")) 
   {
      tile_parser::token_order order;
      if (vm.count("zxy")) 
      {
         order = tile_parser::order_z_x_y;
      }
      else if (vm.count("xyz"))
      {
         order = tile_parser::order_x_y_z;
      }
      else
      {
         cerr << "Must provide either the xyz or zxy order for tiles input." << endl;
      }
      parser = tile_parser(delimiter, order);
//  } else if (vm.count("latlon")) {
//    parser = latlon_parser(delimiter);
//  } else if (vm.count("lonlat")) {
//    parser = lonlat_parser(delimiter);
   } 
   else 
   {
      cerr << "Must provide one of tiles, latlon, lonlat as parameter." << endl;
      return EXIT_FAILURE;
   }

   optional<int> expire_min_z, expire_max_z; //, rerender_min_z, rerender_max_z;
   if (vm.count("expire-down-to")) 
   {
      expire_min_z = vm["expire-down-to"].as<int>();
   }
   if (vm.count("expire-up-to")) 
   {
      expire_max_z = vm["expire-up-to"].as<int>();
   }
/*
   if (vm.count("rerender-down-to")) 
   {
      rerender_min_z = vm["rerender-down-to"].as<int>();
   }
   if (vm.count("rerender-up-to")) 
   {
      rerender_max_z = vm["rerender-up-to"].as<int>();
   }
*/

   int min_z = numeric_limits<int>::max();
   if (expire_min_z) { min_z = std::min(min_z, expire_min_z.get()); }
   //if (rerender_min_z) { min_z = std::min(min_z, rerender_min_z.get()); }
   
   int max_z = numeric_limits<int>::min();
   if (expire_max_z) { max_z = std::max(max_z, expire_max_z.get()); }
   //if (rerender_max_z) { max_z = std::max(max_z, rerender_max_z.get()); }

/*
   scoped_ptr<broker_rerender> rerender;
   if (vm.count("rerender-with")) {
      rerender.reset(
         new broker_rerender(
            vm["rerender-with"].as<string>(),
            rerender_min_z, rerender_max_z));
      sinks.push_back(boost::ref(*rerender));
   }
*/   
   g_verbose = vm.count("verbose") > 0;
   g_perform_actions = vm.count("dry-run") == 0;
   
   if (g_verbose) {
      cout << "# expiring styles: ";
      BOOST_FOREACH(string style, styles) {
         cout << style << ", ";
      }
      cout << endl;
   }

   size_t n_threads = vm["num-threads"].as<size_t>();
   unordered_set<xyz> all_tiles;
   vector<string> files = vm["input-files"].as<vector<string> >();
   BOOST_FOREACH(string file, files) {
      char line[1024];
      
      ifstream in(file.c_str());
      while (in) {
         in.getline(line, sizeof line);
         string str_line(line);
         if (!str_line.empty()) {

            rendermq::tile_protocol tile = parser(str_line);
            {
               // make the xyzs all metatiles, as this is what the 
               // expiry storage function works on - no point expiring
               // each tile as that would create too much work.
               xyz tile_xyz(tile.x & ~(METATILE - 1), 
                            tile.y & ~(METATILE - 1), 
                            tile.z);
               
               const int zmin = std::min(tile.z, min_z);
               const int zmax = std::max(tile.z, max_z);
               child_tiles(tile_xyz, zmax, all_tiles);
               parent_tiles(tile_xyz, zmin, all_tiles);
            }
         }
      }
   }

   // now we have all_tiles, split the tile list up into parallel parts
   // to be done.
   size_t tiles_per_thread = all_tiles.size() / n_threads + 
      ((all_tiles.size() % n_threads) > 0 ? 1 : 0);
   vector<list<xyz> > thread_tiles(n_threads);
   {
      unordered_set<xyz>::iterator tile_itr = all_tiles.begin();
      BOOST_FOREACH(list<xyz> &tile_list, thread_tiles) 
      {
         size_t count = 0;
         while (count < tiles_per_thread && tile_itr != all_tiles.end())
         {
            tile_list.push_back(*tile_itr);
            ++tile_itr;
            ++count;
         }
      }
   }
   // don't need this any more
   LOG_INFO(boost::format("total tile count: %1%") % all_tiles.size());
   all_tiles.clear();

   if (g_verbose) { std::cout << "Starting " << n_threads << " threads..." << std::endl; }
   vector<shared_ptr<boost::thread> > threads(n_threads);
   for (size_t i = 0; i < n_threads; ++i) 
   {
      const list<xyz> &tile_list = thread_tiles[i];
      shared_ptr<boost::thread> &thread = threads[i];

      shared_ptr<boost::thread> th(
         new boost::thread(
            expire_tiles,
            vm["worker-config"].as<string>(),
            expire_min_z, expire_max_z,
            boost::cref(styles), 
            boost::cref(types),
            boost::cref(formats),
            boost::cref(tile_list)));
      
      thread.swap(th);
      if (g_verbose) { std::cout << "Started thread " << i << ", with " << tile_list.size() << " jobs" << std::endl; }
   }

   // join threads
   BOOST_FOREACH(shared_ptr<boost::thread> &thread, threads)
   {
      thread->join();
   }

   LOG_INFO(boost::format("Complete"));
            
   return EXIT_SUCCESS;
}

