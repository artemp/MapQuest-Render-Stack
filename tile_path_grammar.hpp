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


#ifndef TILE_PATH_GRAMMAR_HPP
#define TILE_PATH_GRAMMAR_HPP

#include <boost/spirit/include/qi.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi_omit.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>

#include "tile_protocol.hpp"

BOOST_FUSION_ADAPT_STRUCT(
    rendermq::tile_protocol,
    (std::string, style)
    (int, z)
    (int, x)
    (int, y)
    (rendermq::protoFmt, format)
    (rendermq::protoCmd, status)
)

namespace rendermq {

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

struct formats_ : qi::symbols<char, protoFmt> {
  formats_() {
    add 
      ("png", fmtPNG)
      ("jpg", fmtJPEG)
      ("jpeg", fmtJPEG) // why not, let's have both spellings...
      ("gif", fmtGIF)
      ("json", fmtJSON)
      ;
  }
};

template <typename Iterator>
struct commands_ : qi::grammar<Iterator, protoCmd()> {
  commands_() : commands_::base_type(cmd) {
    using qi::_val;
    using qi::lit;
    using qi::eoi;
    cmd = 
      eoi            [ _val = rendermq::cmdRender ] |
      lit("/dirty")  [ _val = rendermq::cmdDirty  ] | 
      lit("/status") [ _val = rendermq::cmdStatus ]
      ;
  }
  qi::rule<Iterator, protoCmd()> cmd;
};

template <typename Iterator>
struct tile_path_grammar : qi::grammar<Iterator, tile_protocol()> {
  tile_path_grammar()
    : tile_path_grammar::base_type(path) {
    using qi::lit;
    using qi::int_;
    using qi::alpha;
    using qi::alnum;
    using boost::spirit::raw;
    path %= lit("/") >> raw[ (alpha >> *alnum) % lit("/") ]
                     >> lit("/") >> int_ 
                     >> lit("/") >> int_ 
                     >> lit("/") >> int_
                     >> lit(".") >> formats 
                     >> commands;
  }
  
  qi::rule<Iterator, tile_protocol()> path;
  commands_<Iterator> commands;
  formats_ formats;
};

}


#endif //  TILE_PATH_GRAMMAR_HPP
