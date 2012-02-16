/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2010-2 Mapquest, Inc.  All Rights reserved.
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
#include "test/common.hpp"

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <boost/format.hpp>
#include <cmath>

using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::vector;
using rendermq::mongrel_request;

namespace 
{

void assert_equal(const string &actual, const string &expected)
{
   if (actual != expected)
   {
      throw std::runtime_error((boost::format("Expected `%1%' but got `%2%'")
                                % expected % actual).str());
   }
}

void test_escape_handling() 
{
   const string input = 
      "MONGREL2 1208 /layer/search/sic:-541103,541105,581208%5Brgb(162,91,156):255:"
      "rgb(0,0,0):1:120:7%5D/tile 931:{\"PATH\":\"/layer/search/sic:-541103,541105,"
      "581208%5Brgb(162,91,156):255:rgb(0,0,0):1:120:7%5D/tile\",\"x-forwarded-for\""
      ":\"127.0.0.1\",\"accept\":\"*/*\",\"user-agent\":\"curl/7.19.5 (x86_64-unknow"
      "n-linux-gnu) libcurl/7.19.5 OpenSSL/1.0.0e zlib/1.2.5\",\"host\":\"localhost:"
      "8002\",\"cookie\":\"s_pers=%20s_getnr%3D1328879970598-Repeat%7C1391951970598%"
      "3B%20s_nrgvo%3DRepeat%7C1391951970599%3B; psession=\\\"ewSvbhLzV9Ysb+u/Msf1ym"
      "iW8GE=\\\"; a_id=4f3417b20ee20145650d46b6; a_sn=1i2e72rviB3YNcFIgcb32Ld0M10%3"
      "D; a_sg=7K13HDLiiYXMNYCTbprIDQyYcJA%3D; UNAUTHID=1.f94e1b82529911e1962057974c"
      "77f38e.116b; CUNAUTHID=1.f94e1b82529911e1962057974c77f38e.116b; traffic_cam_b"
      "ubble=traffic_cam_bubble; cpcollapsed=1; s_sess=%20s_cc%3Dtrue%3B%20s_sq%3D%3"
      "B\",\"METHOD\":\"GET\",\"VERSION\":\"HTTP/1.1\",\"URI\":\"/layer/search/sic:-"
      "541103,541105,581208%5Brgb(162,91,156):255:rgb(0,0,0):1:120:7%5D/tile?s=13&y="
      "3076&x=2411&p=sm\",\"QUERY\":\"s=13&y=3076&x=2411&p=sm\",\"PATTERN\":\"/layer"
      "/search\"},0:,";
   
   rendermq::request_parser parser;
   rendermq::mongrel_request req;

   if (!parser(req, input))
   {
      throw std::runtime_error("Expected request to parse OK, but didn't");
   }

   assert_equal(req.uuid(), "MONGREL2");
   assert_equal(req.id(), "1208");
   assert_equal(req.path(), "/layer/search/sic:-541103,541105,581208%5B"
                "rgb(162,91,156):255:rgb(0,0,0):1:120:7%5D/tile");
   assert_equal(req.query_string(), "s=13&y=3076&x=2411&p=sm");

   mongrel_request::cont_type::iterator itr = req.headers().find("cookie");
   if (itr == req.headers().end())
   {
      throw std::runtime_error("Expected to find cookie header, but didn't");
   }
   assert_equal(itr->second, 
                "s_pers=%20s_getnr%3D1328879970598-Repeat%7C1391951970598%"
                "3B%20s_nrgvo%3DRepeat%7C1391951970599%3B; psession=\"ewSv"
                "bhLzV9Ysb+u/Msf1ymiW8GE=\"; a_id=4f3417b20ee20145650d46b6"
                "; a_sn=1i2e72rviB3YNcFIgcb32Ld0M10%3D; a_sg=7K13HDLiiYXMN"
                "YCTbprIDQyYcJA%3D; UNAUTHID=1.f94e1b82529911e1962057974c7"
                "7f38e.116b; CUNAUTHID=1.f94e1b82529911e1962057974c77f38e."
                "116b; traffic_cam_bubble=traffic_cam_bubble; cpcollapsed="
                "1; s_sess=%20s_cc%3Dtrue%3B%20s_sq%3D%3B");
}

} // anonymous namespace

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Mongrel Request Parsing ==" << endl << endl;
   
   tests_failed += test::run("test_escape_handling", &test_escape_handling);
   //tests_failed += test::run("test_", &test_);
   
   cout << " >> Tests failed: " << tests_failed << endl << endl;
   
   return 0;
}
