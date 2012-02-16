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

#include <boost/optional.hpp>
#include <boost/format.hpp>
#include "tile_storage.hpp"
#include "../logging/logger.hpp"

namespace rendermq
{

tile_storage::~tile_storage() {}
tile_storage::handle::~handle() {}

bool tile_storage_factory::add(std::string const& type, 
                               tile_storage* (*func) (boost::property_tree::ptree const&,
                                                      boost::optional<zmq::context_t &> ctx))
{
    return cont.insert(std::make_pair(type,func)).second;
}

bool tile_storage_factory::remove(std::string const& type)
{
    return (cont.erase(type) == 1);
}

tile_storage * tile_storage_factory::create(boost::property_tree::ptree const& pt,
                                            boost::optional<zmq::context_t &> ctx)
{
    boost::optional<std::string> type = pt.get_optional<std::string>("type");
    if (type)
    {
        std::map<std::string,storage_creator>::const_iterator pos = cont.find(*type);
        if (pos != cont.end())
        {
           return (pos->second)(pt, ctx);
        }
   
        LOG_ERROR(boost::format("Storage for type '%1%' has not been registered.") % type.get()); 
    }
    else
    {
       LOG_ERROR("Cannot construct storage: configuration does not specify the type.");
    }

    return 0;
}


bool register_tile_storage(std::string const& type, 
                           storage_creator func)
{
    
    return tile_storage_factory::instance()->add(type, func);
}

tile_storage * get_tile_storage(boost::property_tree::ptree const& pt,
                                boost::optional<zmq::context_t &> ctx)
{
   return tile_storage_factory::instance()->create(pt, ctx);
}

}

