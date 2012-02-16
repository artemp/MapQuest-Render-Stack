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

#ifndef HTTP_DATE_GRAMMAR_HPP
#define HTTP_DATE_GRAMMAR_HPP

// qi
#include <boost/spirit/include/qi.hpp>
// fusion
#include <boost/fusion/include/adapt_struct.hpp>
// phoenix 
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/home/phoenix/statement/if.hpp>

#include <string>
#include <ctime>
#include <iostream>

namespace rendermq
{

struct timestamp : tm {};

std::ostream& operator<< (std::ostream& os, const timestamp& t)
{
    os << "timestamp =\n"
       << "    {\n"
       << "    tm_sec   = " << t.tm_sec   << "\n"
       << "    tm_min   = " << t.tm_min   << "\n"
       << "    tm_hour  = " << t.tm_hour  << "\n"
       << "    tm_mday  = " << t.tm_mday  << "\n"
       << "    tm_mon   = " << t.tm_mon   << "\n"
       << "    tm_year  = " << t.tm_year  << "\n"
       << "    tm_wday  = " << t.tm_wday  << "\n"
       << "    tm_yday  = " << t.tm_yday  << "\n"
       << "    tm_isdst = " << t.tm_isdst << "\n"
       << "    }\n";
    return os;
}
}

BOOST_FUSION_ADAPT_STRUCT (
    rendermq::timestamp,
    (int, tm_wday)
    (int, tm_mday)
    (int, tm_mon)
    (int, tm_year)
    (int, tm_hour)
    (int, tm_min)
    (int, tm_sec)
    )

// http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html
// 
// HTTP-date    = rfc1123-date | rfc850-date | asctime-date
// rfc1123-date = wkday "," SP date1 SP time SP "GMT"
// rfc850-date  = weekday "," SP date2 SP time SP "GMT"
// asctime-date = wkday SP date3 SP time SP 4DIGIT
// date1        = 2DIGIT SP month SP 4DIGIT
//           ;day month year (e.g., 02 Jun 1982)
// date2        = 2DIGIT "-" month "-" 2DIGIT
//           ; day-month-year (e.g., 02-Jun-82)
// date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
//           ; month day (e.g., Jun  2)
// time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
//           ; 00:00:00 - 23:59:59
// wkday        = "Mon" | "Tue" | "Wed"
//              | "Thu" | "Fri" | "Sat" | "Sun"
// weekday      = "Monday" | "Tuesday" | "Wednesday"
//        | "Thursday" | "Friday" | "Saturday" | "Sunday"
// month        = "Jan" | "Feb" | "Mar" | "Apr"
//              | "May" | "Jun" | "Jul" | "Aug"
//              | "Sep" | "Oct" | "Nov" | "Dec"



namespace rendermq {

namespace qi = boost::spirit::qi;

struct month : qi::symbols<char,int>
{
    month()
    {
        add
            ("Jan", 0)
            ("Feb", 1)
            ("Mar", 2)
            ("Apr", 3)
            ("May", 4)
            ("Jun", 5)
            ("Jul", 6)
            ("Aug", 7)
            ("Sep", 8)
            ("Oct", 9)
            ("Nov", 10)
            ("Dec", 11)
            ;
    }
};

struct wk_day : qi::symbols<char,int>
{
    wk_day()
    {
        add
            ("Mon", 1)
            ("Tue", 2)
            ("Wed", 3)
            ("Thu", 4)
            ("Fri", 5)
            ("Sat", 6)
            ("Sun", 0)
            ;
    }
};


struct week_day : qi::symbols<char,int>
{
    week_day()
    {
        add
            ("Monday", 1)
            ("Tuesday", 2)
            ("Wednesday", 3)
            ("Thursday", 4)
            ("Friday", 5)
            ("Saturday", 6)
            ("Sunday", 0)
            ;
    }
};

template <typename Iterator>
struct http_date_grammar
    : qi::grammar<Iterator, timestamp(), qi::space_type>
{
    http_date_grammar()
      : http_date_grammar::base_type(http_date)
    {
        using qi::lit;
        using qi::_val;
        using qi::_1;
        using boost::phoenix::at_c;
        using boost::phoenix::if_;
        
        http_date %= rfc1123_date | rfc850_date | asctime_date;
        rfc1123_date = wd [at_c<0>(_val) = _1] 
            >> lit(",") 
            >> uint2 [at_c<1>(_val) = _1] 
            >> m [at_c<2>(_val) = _1] >> uint4[at_c<3>(_val) = _1 - 1900u]
            >> uint2 [at_c<4>(_val) = _1] >> lit(":") >> uint2 [at_c<5>(_val) = _1] 
            >> lit(":") >> uint2 [at_c<6>(_val) = _1]>> lit("GMT") ;
        
        rfc850_date = weekday [at_c<0>(_val) = _1] 
            >> lit(",") 
            >> uint2 [at_c<1>(_val) = _1] >> lit("-") 
            >> m [at_c<2>(_val) = _1] >> lit("-") 
            >> uint2 [if_(_1 < 70u) [ at_c<3>(_val) = _1 + 100u ].else_[ at_c<3>(_val) = _1 ]]
            >> uint2 [at_c<4>(_val) = _1] >> lit(":") >> uint2 [at_c<5>(_val) = _1] 
            >> lit(":") >> uint2 [at_c<6>(_val) = _1]>> lit("GMT") ;            

        asctime_date = wd [at_c<0>(_val) = _1]
            >> m [at_c<2>(_val) = _1] >> uint1_2 [at_c<1>(_val) = _1]
            >> uint2 [at_c<4>(_val) = _1] >> lit(":") >> uint2 [at_c<5>(_val) = _1] 
            >> lit(":") >> uint2 [at_c<6>(_val) = _1] >> uint4[at_c<3>(_val) = _1 - 1900u];
    }
    
    qi::rule<Iterator, timestamp(), qi::space_type> http_date;
    qi::rule<Iterator, timestamp(), qi::space_type> rfc1123_date;
    qi::rule<Iterator, timestamp(), qi::space_type> rfc850_date;
    qi::rule<Iterator, timestamp(), qi::space_type> asctime_date;
    qi::uint_parser< unsigned, 10, 1, 2 > uint1_2;
    qi::uint_parser< unsigned, 10, 2, 2 > uint2;
    qi::uint_parser< unsigned, 10, 4, 4 > uint4;
    wk_day wd;
    week_day weekday;
    month m;
};

}

#endif // HTTP_DATE_GRAMMAR_HPP
