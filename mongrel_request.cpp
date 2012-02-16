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

#include "mongrel_request.hpp"
#include "http/http_date_parser.hpp"

namespace rendermq {

void mongrel_request::set_uuid(std::string const& uuid)
{
    uuid_ = uuid;
}
    
void mongrel_request::set_path(std::string const& path)
{
    path_ = path;
}
    
void mongrel_request::set_id(std::string const& id)
{
    id_ = id;
}

std::string mongrel_request::query_string() const
{
   cont_type::const_iterator q_itr = headers().find("QUERY");
   if (q_itr != headers().end()) 
   {
      return q_itr->second;
   }
   else
   {
      return std::string();
   }
}

bool mongrel_request::is_disconnect() const
{
    return false;
}

bool mongrel_request::if_modified_since(std::time_t modified) const
{
    cont_type::const_iterator itr = headers_.find("if-modified-since");
    if (itr != headers_.end())
    {
        std::time_t time;
        if (parse_http_date(time, itr->second))
        {
            return (time > modified) ; 
        }
    }
    return true;
}

}
