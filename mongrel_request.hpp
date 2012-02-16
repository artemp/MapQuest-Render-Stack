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

#ifndef MONGREL_REQUEST_HPP
#define MONGREL_REQUEST_HPP

#include <iostream>
#include <map>
#include <ctime> // for std::time_t

namespace rendermq  {

class mongrel_request 
{
public:
    typedef std::map<std::string,std::string> cont_type;

    mongrel_request() {}
    void set_uuid(std::string const& uuid);
    void set_id(std::string const& id);
    void set_path(std::string const& path);
    
    std::string const& uuid() const { return uuid_;}
    std::string const& id() const { return id_;}
    std::string const& path() const { return path_;}
   // gets the query string, or a blank string if the query string
   // isn't defined.
   std::string query_string() const;

    inline cont_type const& headers() const { return headers_; }
    inline cont_type & headers() {return headers_;}
    inline cont_type & body() { return body_; }
    inline cont_type const& body() const { return body_; }
    
    bool is_disconnect() const;
    bool if_modified_since(std::time_t modified) const;
    
private:
    std::string uuid_;
    std::string id_;
    std::string path_;
    cont_type headers_;
    cont_type body_;
};


}

#endif // MONGREL_REQUEST_HPP
