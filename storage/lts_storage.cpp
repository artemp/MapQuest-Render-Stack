/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: john.novak@mapquest.com
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

// default driver parameters can be overridden by the
// storage section of the handler / worker configs.
#define DEFAULT_CONCURRENCY (16) //how many HTTP connections to open to the back-end
#define DEFAULT_VERSION "0"
#define DEFAULT_DOWN_RECHECK_TIME (300) // how often to recheck that a down LTS host is still down.

// 300ms timeout for connect (just the TCP handshake - not the whole HTTP
// transaction) to help prevent the storage_worker getting bogged down in
// the event of an LTS node failure.
#define LTS_CONNECT_TIMEOUT (300L)

#include "lts_storage.hpp"
#include "../tile_utils.hpp"
#include <time.h>

#include <boost/foreach.hpp> //for each macro
#include <boost/algorithm/string/split.hpp> //split
#include <boost/algorithm/string/erase.hpp> //erase_all
#include <boost/algorithm/string/classification.hpp> //is_any_of
#include <boost/algorithm/string/constants.hpp> //token_compress_on
#include <boost/lexical_cast.hpp> //lexical_cast
//#include <boost/algorithm/string.hpp> //str

namespace rendermq
{
   namespace
   {
      tile_storage * create_lts_storage(boost::property_tree::ptree const& pt,
                                        boost::optional<zmq::context_t &> ctx)
      {
         boost::optional<string> hosts = pt.get_optional<string>("hosts"); //should look like hosts = host:port,host:port
         boost::optional<string> config = pt.get_optional<string>("config"); //config = --DISTRIBUTION=consistent --HASH=MURMUR
         boost::optional<string> app_name = pt.get_optional<string>("app_name");
         string version = pt.get<string>("version", DEFAULT_VERSION);
         unsigned int concurrency = pt.get<unsigned int>("concurrency", DEFAULT_CONCURRENCY);
         int down_recheck_time = pt.get<unsigned int>("down_recheck_time", DEFAULT_DOWN_RECHECK_TIME);

         vecHostInfo vecHosts;
         if(hosts)
         {
            //remove all white space
            boost::algorithm::erase_all(*hosts, " ");
            boost::algorithm::erase_all(*hosts, "\t");
            boost::algorithm::erase_all(*hosts, "http://");
            //split on ,
            vector<string> hostPort;
            boost::algorithm::split(hostPort, *hosts, boost::algorithm::is_any_of(","), boost::algorithm::token_compress_on);
            //for each "host:port"
            for(vector<string>::const_iterator machine = hostPort.begin(); machine != hostPort.end(); machine++)
            {
               //split on :
               vector<string> separated;
               boost::algorithm::split(separated, *machine, boost::algorithm::is_any_of(":"), boost::algorithm::token_compress_on);
               //if it has something:something else
               if(separated.size() == 2)
               {
                  try
                  {
                     //case the port to int and store it in the vector
                     vecHosts.push_back(std::make_pair(separated[0], boost::lexical_cast<int>(separated[1])));
                  }
                  catch(...){}
               }
            }
         }

         //required
         if(vecHosts.size() && config && app_name)
         {
            //make sure that it has hosts to write to
            lts_storage* storage = new lts_storage(vecHosts, *config, *app_name, version, concurrency, down_recheck_time);
            if(storage->getHostCount())
               return storage;
            else
               return 0;
         }
         return 0;
      }

      const bool registered = register_tile_storage("lts", create_lts_storage);

   } // anonymous namespace


   lts_storage::lts_storage(const vecHostInfo& vecHosts, const string& config, const string& app_name, const string& version, const int& concurency, int down_recheck_time):
      http_storage(false, concurency), app_name(app_name), version(version),
      m_down_recheck_time(down_recheck_time)
   {
      this->pHashWrapper = boost::make_shared<hashWrapper>(config, vecHosts);
   }

   lts_storage::~lts_storage()
   {
   }

   std::pair<string, int> lts_storage::hashed_host(int x, int y, int z, unsigned int replica) const
   {
      //get the host for this zxy
      std::stringstream xyz;
      xyz << z << "/" << x << "/" << y;
      return this->pHashWrapper->getHost(xyz.str(), replica);
   }

   const string lts_storage::form_url(const int& x, const int& y, const int& z, const string& style, const protoFmt& format, const unsigned int& replica) const
   {
      std::pair<string, int> hashedHost = hashed_host(x, y, z, replica);

      //create the url
      std::stringstream stream;
      stream << "http://" << hashedHost.first << ":" << hashedHost.second << "/" << version << "/" << this->app_name << style << "/" << z << "/" << x << "/" << y << "." << file_type_for(format);
      return stream.str();
   }

   void lts_storage::resync_tile(const tile_protocol &tile, const string& data, const long& timeStamp, const int& replica) const
   {
      //make the header and the requests
      vector<string> headers = this->make_headers(&timeStamp, boost::str(boost::format("X-Replica: %1%") % replica).c_str(), (char*)NULL);
      vector<pair<string, vector<http::part> > > requests;

      //get the url to post to and mimic html form post
      string url = this->form_url(tile.x, tile.y, tile.z, tile.style, tile.format, replica);
      vector<http::part> parts;
      parts.push_back(http::part(NULL, 0));
      http::part* part = &parts[0];
      part->name = "file";
      part->mime = rendermq::mime_type_for(tile.format).c_str();;
      part->data = data.c_str();
      part->size = (long)data.length();
      part->position = 0;
      part->fileName = url.c_str();
      requests.push_back(make_pair(url, parts));

      //put the tile
      put_meta_serial(requests, headers);
   }

   bool lts_storage::is_host_down(const std::pair<string, int> &host) const
   {
      std::map<std::pair<string, int>, time_t>::iterator itr = m_hosts_down.find(host);
      if (itr != m_hosts_down.end())
      {
         time_t now = time(NULL);
         if (now > itr->second)
         {
            // time has elapsed - remove the record and treat as if it were
            // not down.
            m_hosts_down.erase(itr);
            return false;
         }
         else
         {
            return true;
         }
      }
      return false;
   }

   void lts_storage::host_is_down(const std::pair<string, int> &host) const
   {
      time_t recheck_time = time(NULL) + m_down_recheck_time;
      std::map<std::pair<string, int>, time_t>::iterator itr = m_hosts_down.find(host);
      if (itr == m_hosts_down.end())
      {
         m_hosts_down.insert(make_pair(host, recheck_time));
      }
      else
      {
         itr->second = recheck_time;
      }
   }

   shared_ptr<http::response> lts_storage::attempt_get_host(const tile_protocol &tile, int replica) const
   {
      shared_ptr<http::response> response;
      std::pair<string, int> hashedHost = hashed_host(tile.x, tile.y, tile.z, replica);

      // if the host is down, then don't bother trying again - it's just
      // a waste of time and blocks other requests in the queue.
      if (!is_host_down(hashedHost))
      {
         // try to get the tile
         try
         {
            //make the url
            string url = this->form_url(tile.x, tile.y, tile.z, tile.style, tile.format, replica);

            // make the header - need for accessing the correct replica
            vector<string> headers;
            headers.push_back((boost::format("X-Replica: %1%") % replica).str());

            //curl to get the tile data, returns a shared_ptr, forget the last shared pointer we had
            response = http::get(url, boost::shared_ptr<CURL>(), headers, false, LTS_CONNECT_TIMEOUT);

            int status_code = response->statusCode;
            if(status_code != 200)
            {
               response.reset();

               // status code 404 (not found) is a perfectly normal runtime condition
               // and doesn't need logging. anything else is interesting and wants to
               // be logged.
               if (status_code != 404)
               {
                  LOG_WARNING((boost::format("getting LTS tile returned status code %1%") % status_code).str());
               }
            }
         }//couldn't get to the hosts
         catch(const std::exception &e)
         {
            LOG_ERROR(boost::format("Runtime error getting LTS tile %1% from LTS host %2%, marking host as down. Error was: %3%") % tile % hashedHost.first % e.what());
            host_is_down(hashedHost);
         }
      }

      return response;
   }

   shared_ptr<tile_storage::handle> lts_storage::get(const tile_protocol &tile) const
   {
      //for getting the response
      shared_ptr<http::response> response;

      //try to get the primary copy
      response = attempt_get_host(tile, 0);

      if (!response) 
      {
         //try to get the secondary copy
         response = attempt_get_host(tile, 1);
      }

      if (!response)
      {
         //return a bad response
         // no logging here - this happens when we get a 404, which is a
         // perfectly normal situation.
         response = shared_ptr<http::response>(new http::response());
      }

      //return the response, good or bad
      return shared_ptr<tile_storage::handle> (new handle(response));
   }

   bool lts_storage::get_meta(const tile_protocol &tile, string &metatile) const
   {
      //get the requests
      vector<string> headers = this->make_headers(NULL, "X-Replica: 0", (char*)NULL);
      vector<string> requests = make_get_urls(tile, true);
      //place to keep the responses
      vector<shared_ptr<http::response> > responses0;

      //try to get the first copy
      bool ret0 = (concurrency < 2 ? get_meta_serial(requests, headers, responses0) : get_meta_parallel(requests, headers, responses0));

      //if we failed for any reason
      if(ret0 == false)
      {
         //try to get the second copy
         headers = this->make_headers(NULL, "X-Replica: 1", (char*)NULL);
         requests = make_get_urls(tile, false);
         vector<shared_ptr<http::response> > responses1;
         bool ret1 = (concurrency < 2 ? get_meta_serial(requests, headers, responses1) : get_meta_parallel(requests, headers, responses1));
         //if this one didn't get them all either
         if(ret1 == false)
         {
            //see if we can get the full set by combining the two
            vector<shared_ptr<http::response> > responsesCombined;
            vector<shared_ptr<http::response> >::const_iterator response0, response1;
            for(response0 = responses0.begin(), response1 = responses1.begin(); response0 != responses0.end(); ++response0, ++response1)
            {
               //first replica is good
               if((*response0)->statusCode == 200 && (*response0)->timeStamp != INVALID_TIMESTAMP)
                  responsesCombined.push_back(*response0);
               //second replica is good
               else if((*response1)->statusCode == 200 && (*response1)->timeStamp != INVALID_TIMESTAMP)
                  responsesCombined.push_back(*response1);
               //both replicas are bad
               else
                  return false;
            }
            //if we got here that means we have a combined one
            make_metatile(tile, responsesCombined, metatile);
         }//we are good with the replica
         else
         {
            //make the metatile out of it
            make_metatile(tile, responses1, metatile);
         }
      }//make the metatile out of the original response
      else
         make_metatile(tile, responses0, metatile);

      //if we made it here we had enough to make the metatile
      return true;
   }

   bool lts_storage::put_meta(const tile_protocol &tile, const string &metatile) const
   {
      //put extra stuff in the http header
      std::time_t now = std::time(0);
      vector<string> headers = this->make_headers(&now, "X-Replica: 0", (char*)NULL);
      //get the put requests
      vector<pair<string, vector<http::part> > > requests = make_put_requests(tile, metatile);

      //put the first copy
      bool ret1 = (concurrency < 2 ? put_meta_serial(requests, headers) : put_meta_parallel(requests, headers));

      //update the requests to the replica copies
      vector<string> secondaryUrls = make_replica_urls(tile, metatile);
      headers = this->make_headers(&now, "X-Replica: 1", (char*)NULL);
      vector<pair<string, vector<http::part> > >::iterator request = requests.begin();
      vector<string>::const_iterator secondaryUrl = secondaryUrls.begin();
      for(; request != requests.end(); request++, secondaryUrl++) {
         request->first = *secondaryUrl;
         request->second.front().fileName = request->first.c_str();
         //curl uses this internally to see how much of the data has been sent
         request->second.front().position = 0;
      }

      //put the second copy
      bool ret2 = (concurrency < 2 ? put_meta_serial(requests, headers) : put_meta_parallel(requests, headers));

      //its only a failure if both copies fail, just make the tile dirty don't waste time deleting it
      if(!ret1 && !ret2)
         this->expire(tile);

      //return whether we were successful in writing either of the copies
      return ret1 || ret2;
   }

   vector<pair<string, vector<http::part> > > lts_storage::make_put_requests(const tile_protocol &tile, const string &metatile) const
   {
      //the requests
      vector<pair<string, vector<http::part> > > requests;
      //read meta header to get the tile offsets and sizes
      vector<meta_layout*> metaHeaders = read_headers(metatile, fmtAll);
      //figure out the meta tile coordinate
      pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);

      //for each format
      for(vector<meta_layout*>::const_iterator metaHeader = metaHeaders.begin(); metaHeader != metaHeaders.end(); metaHeader++)
      {
         //get the format
         protoFmt format = (protoFmt)(*metaHeader)->fmt;
         //save the mime type
         const char* mime = rendermq::mime_type_for(format).c_str();
         //for each tile
         for(int i = 0; i < (*metaHeader)->count; i++)
         {
            //only send this if there is an actual tile here
            if((*metaHeader)->index[i].size == 0)
               continue;

            //get the url to post to
            string url = this->form_url(coord.first + (i % METATILE), coord.second + (i / METATILE), tile.z, tile.style, format);

            //have to mimic html form post
            vector<http::part> parts;
            parts.push_back(http::part(NULL, 0));
            http::part* part = &parts[0];
            part->name = "file";
            part->mime = mime;
            part->data = metatile.c_str() + (*metaHeader)->index[i].offset;
            part->size = (long)(*metaHeader)->index[i].size; //kind of shady using pointer from a reference parameter
            part->position = 0;
            part->fileName = url.c_str(); //required to trick curl into <input type=file> instead of <input type=text>
            //printf("%s -> %s\n", part->fileName, string(part->data, part->size).c_str());
            requests.push_back(make_pair(url, parts));
         }
      }

      //give them back
      return requests;
   }

   vector<string> lts_storage::make_get_requests(const tile_protocol &tile) const
   {
      return this->make_get_urls(tile, true);
   }

   vector<string> lts_storage::make_get_urls(const tile_protocol &tile, bool is_primary) const
   {
      //get the master tile location
      pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);
      //figure out how many sub tiles it will have
      int dim = get_meta_dimensions(tile.z);
      vector<string> urls;
      vector<protoFmt> fmts = get_formats_vec(tile.format);

      BOOST_FOREACH(protoFmt fmt, fmts)
      {
         for (int dy = 0; dy < dim; ++dy)
         {
            for (int dx = 0; dx < dim; ++dx)
            {
               //make the urls
               urls.push_back(form_url(coord.first + dx, coord.second + dy, tile.z, tile.style, fmt, int(!is_primary)));
            }
         }
      }

      return urls;
   }

   vector<string> lts_storage::make_replica_urls(const tile_protocol &tile, const string &metatile) const
   {
      //read meta header to get the tile offsets and sizes
      vector<meta_layout*> metaHeaders = read_headers(metatile, fmtAll);
      //figure out the meta tile coordinate
      pair<int, int> coord = xy_to_meta_xy(tile.x, tile.y);
      //place to keep the urls
      vector<string> replicaUrls;

      //for each format
      for(vector<meta_layout*>::const_iterator metaHeader = metaHeaders.begin(); metaHeader != metaHeaders.end(); metaHeader++)
      {
         //get the format
         protoFmt format = (protoFmt)(*metaHeader)->fmt;
         //TODO: make this get the secondary server for each tile
         for(int i = 0; i < (*metaHeader)->count; i++)
            //only send this if there is an actual tile here
            if((*metaHeader)->index[i].size != 0)
               replicaUrls.push_back(this->form_url(coord.first + (i % METATILE), coord.second + (i / METATILE),
                  tile.z, tile.style, format, 1));
      }

      //send back the urls
      return replicaUrls;
   }

   vector<string> lts_storage::expiry_headers(bool is_primary) const
   {
      static const char * primary_hdr = "X-Replica: 0";
      static const char * replica_hdr = "X-Replica: 1";
      std::time_t invalid = INVALID_TIMESTAMP;
      return make_headers(&invalid, is_primary ? primary_hdr : replica_hdr, (char*)NULL);
   }

   bool lts_storage::expire(const tile_protocol &tile) const
   {
      //do a get with time stamp set to invalid
      //next time its requested timestamp will look dirty and force a rerender
      //meanwhile we can still serve it to clients regardless of whether its dirty or not
      vector<string> primaryUrls = make_get_urls(tile, true), replicaUrls = make_get_urls(tile, false);

      //use invalid time stamp to mark as dirty
      const vector<string> primaryHeaders = expiry_headers(true);
      const vector<string> replicaHeaders = expiry_headers(false);

      //parallelize the requests
      try
      {
         //expire primary copy
         http::multiGet(primaryUrls, concurrency, connection, primaryHeaders);
         //expire replica copy
         http::multiGet(replicaUrls, concurrency, connection, replicaHeaders);
      }
      catch(std::runtime_error e)
      {
         LOG_ERROR(boost::format("Runtime error while expiring LTS tile: %1%") % e.what());
         return false;
      }
      return true;
   }

}
