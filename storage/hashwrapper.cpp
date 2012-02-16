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

#include "meta_tile.hpp"
#include "hashwrapper.hpp"
#include "hss_storage.hpp"

namespace rendermq
{

	class MemcacheConstructionException : public std::runtime_error 
	{
		public:
			MemcacheConstructionException() : std::runtime_error("MemcacheConstructionException") { }
	};
	
	hashWrapper::hashWrapper(const std::string &config, const vecHostInfo &hosts)
	{
		this->pMemc = memcached(config.c_str(), config.length());
		
		if(!this->pMemc )
			throw MemcacheConstructionException();

		serverCount = 0;
		for(citerVecHostInfo host = hosts.begin(); host != hosts.end(); host++)
			if(memcached_server_add(this->pMemc, (*host).first.c_str(), (*host).second) == MEMCACHED_SUCCESS)
			   serverCount++;
		//
		//  Do not change ordering of options below; it is significant
		//
		memcached_behavior_set(this->pMemc, MEMCACHED_BEHAVIOR_KETAMA, 0);
		memcached_behavior_set(this->pMemc, MEMCACHED_BEHAVIOR_KETAMA_WEIGHTED, 0);
		memcached_behavior_set(this->pMemc, MEMCACHED_BEHAVIOR_KETAMA_HASH, MEMCACHED_HASH_DEFAULT);
		memcached_behavior_set(this->pMemc, MEMCACHED_BEHAVIOR_DISTRIBUTION, MEMCACHED_DISTRIBUTION_CONSISTENT);
		memcached_behavior_set(this->pMemc, MEMCACHED_BEHAVIOR_HASH, MEMCACHED_HASH_FNV1A_32);
	
		this->hashType = HASHKIT_HASH_DEFAULT;
	}
	
	hashWrapper::~hashWrapper()
	{
		memcached_free(this->pMemc);
	}
	
	std::pair<std::string, int> hashWrapper::getHost(const std::string &hashValue, const unsigned int& offset)
	{
	   //check that we have a list of servers to hash to
		if(!this->pMemc)
			throw MemcacheConstructionException();
		memcached_return_t memcRC;

      //get a pointer to the first host in the list of hosts
		const memcached_server_st* first = memcached_server_instance_by_position(this->pMemc, 0);
      //hash this value to pointer to its host in the list of hosts
		const memcached_server_st* original = memcached_server_by_key(this->pMemc, hashValue.c_str(), hashValue.length(), &memcRC); //TODO: if(memcRc != MEMCACHED_SUCCESS) throw()
		//get the host based on offset from the hashed host (useful for replication across multiple hosts)
		const memcached_server_st* selected = first + (((original - first) + offset) % serverCount);
		//give back the host and port
		return std::make_pair(selected->hostname, selected->port);
	}
			
	void hashWrapper::setHashType(hashkit_hash_algorithm_t hashType)
	{
		this->hashType	= hashType;
		
      this->pHashKit.reset(new Hashkit());
	}
	
	hashkit_hash_algorithm_t hashWrapper::getHashType()
	{
		return this->hashType;
	}
	
	uint32_t hashWrapper::getHash(std::string &hashValue)
	{
		if(!pHashKit)
         this->pHashKit = boost::make_shared<Hashkit>();
			
		pHashKit->set_function(this->hashType);
		return pHashKit->digest(hashValue);
	}

};
