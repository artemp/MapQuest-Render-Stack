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

#include "http_date_parser.hpp"
#include "http_date_grammar.hpp"
#include <time.h>

namespace rendermq
{

bool parse_http_date(std::time_t & time, std::string const& input)
{
    typedef std::string::const_iterator iterator_type;
    typedef http_date_grammar<iterator_type> http_date_grammar;    
    iterator_type first = input.begin();
    iterator_type last = input.end();
    timestamp t;
    memset(&t,0,sizeof(timestamp));
    
    http_date_grammar g;
    bool result = boost::spirit::qi::phrase_parse(first,
                                                  last,
                                                  g,
                                                  boost::spirit::qi::space,
                                                  t);
    
    // NOTE: timegm may not be available on all platforms. if it
    // isn't available then remember that *time* is since the epoch
    // in GMT and so is *t* - neither of them are local times!
    if (result) time = timegm(&t);
    return result;
}

}
