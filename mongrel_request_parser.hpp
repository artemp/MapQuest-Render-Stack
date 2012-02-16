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

#ifndef MONGREL_REQUEST_PARSER_HPP
#define MONGREL_REQUEST_PARSER_HPP

// mapnik
#include "mongrel_request.hpp"
// boost
#include <boost/scoped_ptr.hpp>
#include <boost/noncopyable.hpp>
// stl
#include <string>

namespace rendermq {

class request_parser : boost::noncopyable
{
public:
   request_parser();
   ~request_parser();
   bool operator() (mongrel_request & request, std::string const& input) const;

private:
   struct pimpl;
   boost::scoped_ptr<pimpl> impl;
};

}

#endif // MONGREL_REQUEST_PARSER_HPP
