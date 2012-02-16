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

#ifndef RENDERMQ_LTS_STORAGE_HPP
#define RENDERMQ_LTS_STORAGE_HPP

#include "http_storage.hpp"
#include "hashwrapper.hpp"

namespace rendermq
{

   class lts_storage: public http_storage
   {
      public:

         lts_storage(const vecHostInfo& vecHosts,const string& config, const string& app_name, const string& version, const int& concurrency = 1, int down_recheck_time = 300);
         virtual ~lts_storage();
         //get a single tile in a single format
         virtual boost::shared_ptr<tile_storage::handle> get(const tile_protocol &tile) const;
         //get each tile in each format and constructs a metatile from them
         virtual bool get_meta(const tile_protocol &tile, string &metatile) const;
         //put each tile in each format by deconstructing a metatile
         virtual bool put_meta(const tile_protocol &tile, const string &metatile) const;
         //expires a tile by setting last modified to invalid (easiest way to expire them)
         virtual bool expire(const tile_protocol &tile) const;
         //returns the total number of hashable hosts
         virtual unsigned int getHostCount() const {return pHashWrapper->getHostCount();}

      // note: this section for "special" LTS expiry
      public:
         std::vector<std::string> expiry_headers(bool is_primary) const;
         //lame we have to do this because of pure virtual make_get_requests needing extra parameters, could use a redesign...
         std::vector<std::string> make_get_urls(const tile_protocol &tile, bool is_primary) const;

      protected:
         //constructs an lts url for a single tile
         virtual const string form_url(const int& x, const int& y, const int& z, const string& style, const protoFmt& format, const unsigned int& replica = 0) const;
         //generate the requests for the put
         virtual vector<pair<string, vector<http::part> > > make_put_requests(const tile_protocol &tile, const string &metatile) const;
         //generate the requests for the get
         virtual std::vector<std::string> make_get_requests(const tile_protocol &tile) const;
         //generate the urls for the replica copies
         virtual vector<string> make_replica_urls(const tile_protocol &tile, const string &metatile) const;
         //if we fail on getting the primary we can resync the tile to it after getting it from the secondary
         void resync_tile(const tile_protocol &tile, const string& data, const long& timeStamp, const int& replica) const;

         // attempt a get to a host, checking the current list of blocked hosts
         // and return a shared pointer to the result, or an empty shared pointer
         // (null) on error.
         boost::shared_ptr<http::response> attempt_get_host(const tile_protocol &tile, int replica) const;

         // make the host for a particular tile and replica
         std::pair<string, int> hashed_host(int x, int y, int z, unsigned int replica) const;

         // check if a host has been marked down or not
         bool is_host_down(const std::pair<string, int> &host) const;
         // mark a host as down, which will prevent it being checked for some time period
         void host_is_down(const std::pair<string, int> &host) const;

         shared_ptr<hashWrapper> pHashWrapper;
         const string app_name;
         const string version;
         const int m_down_recheck_time;

         /* functor used as a comparator so that hostname:port pairs can
          * be stored in a std::map.
          */
         struct cmp_pair
         {
            inline bool operator()(const std::pair<string,int> &a,
                                   const std::pair<string,int> &b) const
            {
               return ((a.first < b.first) ||
                       (a.first == b.first && a.second < b.second));
            }
         };

         // maps the host into the time it was last checked as down
         mutable std::map<std::pair<string,int>, time_t, cmp_pair> m_hosts_down;
   };

}

#endif // RENDERMQ_LTS_STORAGE_HPP
