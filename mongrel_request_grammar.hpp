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

#ifndef MONGREL_REQUEST_GRAMMAR_HPP
#define MONGREL_REQUEST_GRAMMAR_HPP

#include "mongrel_request.hpp"
#include "tile_protocol.hpp"

#define BOOST_SPIRIT_UNICODE
#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/std_pair.hpp>

namespace rendermq {

namespace qi = boost::spirit::qi;
namespace unicode = boost::spirit::unicode;

// single char escapes which are allowed by the JSON
// specification, mapping to their ASCII/UTF-8 codes.
struct escapes : qi::symbols<char, char>
{
   escapes() 
   {
      add
         ("\"", 0x22)
         ("\\", 0x5c)
         ("/",  0x2f)
         ("b",  0x08)
         ("f",  0x0c)
         ("n",  0x0a)
         ("r",  0x0d)
         ("t",  0x09)
         ;
   }
};

template <typename Iterator>
struct keys_and_values
   : qi::grammar<Iterator, std::map<std::string, std::string>(), qi::space_type>
{
   keys_and_values()
      : keys_and_values::base_type(query)
   {
      query = pair % ',';
      pair  = str >> ':' >> str;
      str   = qi::lit('"') >> *uchar >> qi::lit('"');
      // JSON spec disallows raw " or \ chars in the string - they must
      // be escaped.
      uchar = (unicode::char_ - '"' - '\\') | (qi::lit('\\') >> esc);
      // NOTE: technically, we're allowed to have 4-hex-digit unicode
      // escapes with \uXXXX, but since we internally only bother with
      // ascii, this doesn't seem to be worth implementing.
   }
   qi::rule<Iterator, std::map<std::string, std::string>(),qi::space_type> query;
   qi::rule<Iterator, std::pair<std::string, std::string>(),qi::space_type> pair;
   qi::rule<Iterator, std::string()> str;
   qi::rule<Iterator, char()> uchar;
   escapes esc;
};

}


#endif //  MONGREL_REQUEST_GRAMMAR_HPP
