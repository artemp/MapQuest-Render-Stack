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

#include <string>
#include <boost/spirit/include/qi.hpp>
#include <boost/algorithm/string/find.hpp>
#include "tile_path_parser.hpp"
#include "tile_path_grammar.hpp"

namespace rendermq {

namespace qi = boost::spirit::qi;

struct tile_path_parser::pimpl {
   tile_path_grammar<std::string::const_iterator> grammar_;      
};

tile_path_parser::tile_path_parser()
   : impl(new pimpl) {
}

tile_path_parser::~tile_path_parser() {
}

bool tile_path_parser::operator() (tile_protocol & tile, std::string const& path) const
{
   typedef boost::iterator_range<std::string::const_iterator> range_type;    
   range_type input(path.begin(),path.end());
   // drop the first two path segments
   range_type r = boost::find_nth(input,std::string("/"),2);
   bool result = qi::parse(r.begin(), input.end(), impl->grammar_, tile);
   return result;
}

}
