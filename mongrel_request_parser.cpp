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

#include "mongrel_request_parser.hpp"
#include "mongrel_request_grammar.hpp"
// boost
#include <boost/foreach.hpp>
// boost
#include <boost/xpressive/xpressive.hpp>
#include "logging/logger.hpp"

namespace rendermq
{

struct request_parser::pimpl {
   boost::xpressive::sregex rex_;
   keys_and_values<std::string::iterator> kv_grammar_;
}; 

request_parser::request_parser()
   : impl(new pimpl) {
   impl->rex_ = boost::xpressive::sregex::compile( "([\\w-]+) (\\d+) (.*) \\d+:\\{(.*)\\},\\d+:,");
}

request_parser::~request_parser() {
}

bool request_parser::operator() (mongrel_request & request, std::string const& input) const
{
   using namespace boost::xpressive;
   LOG_FINER(boost::format("INPUT = `%1%'") % input);
   smatch what;
   if( regex_match( input, what, impl->rex_ ))
   {
      request.set_uuid(what[1].str());
      request.set_id(what[2].str());
      request.set_path(what[3].str());
      std::string headers(what[4].str());
      std::string::iterator begin = headers.begin();
      std::string::iterator end = headers.end();
        
      bool result = qi::phrase_parse(begin, end, impl->kv_grammar_, qi::space, request.headers());   // returns true if successful
      if (result)
      {
#ifdef RENDERMQ_DEBUG
         LOG_FINER("------------------ HTTP HEADERS ------------------");
         BOOST_FOREACH(mongrel_request::cont_type::value_type &v,request.headers())
         {
            LOG_FINER(boost::format("%1%:%2%") % v.first % v.second);
         }
         LOG_FINER("--------------------------------------------------");
#endif
         return true;
      }
      else
      {
         LOG_ERROR(boost::format("Failed to parse headers: %1%") % headers) ;
      }
   }
   return false;
}

}
