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

#ifndef HTTP_DATE_FORMATTER_HPP
#define HTTP_DATE_FORMATTER_HPP

#include <boost/date_time/local_time/local_time.hpp>

namespace rendermq
{

using namespace boost::posix_time;
using namespace boost::gregorian;
using namespace boost::local_time;

class http_date_formatter
{
public:
    
    http_date_formatter()
        : zone_(new posix_time_zone("GMT"))
    {}
    
    // RFC 1123 HTTP-date
    template <typename Out>
    inline void operator() (Out & ss, std::time_t const& tt) const
    {
        ptime pt = from_time_t(tt);
        local_time_facet * output_facet = new local_time_facet;
        output_facet->format("%a, %d %b %Y %H:%M:%S %z");  
        ss.imbue(std::locale(std::locale::classic(), output_facet));        
        local_date_time ldt(pt,zone_);
        ss << ldt;
    }
    
    time_zone_ptr zone_;
};

}


#endif //HTTP_DATE_FORMATTER_HPP
