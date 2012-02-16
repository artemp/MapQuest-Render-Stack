/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: matt.amos@mapquest.com
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

#include "tile_path_parser.hpp"
#include "tile_protocol.hpp"
#include "test/common.hpp"
#include "http/http_date_parser.hpp"
#include "http/http_date_formatter.hpp"
#include <stdexcept>
#include <iostream>
#include <iterator>
#include <map>
#include <limits>
#include <boost/function.hpp>
#include <boost/format.hpp>

using rendermq::tile_path_parser;
using rendermq::tile_protocol;
using boost::function;
using boost::optional;
using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::map;
using std::numeric_limits;

using rendermq::cmdRender;
using rendermq::cmdDirty;
using rendermq::cmdStatus;
using rendermq::fmtPNG;
using rendermq::fmtJPEG;
using rendermq::fmtJSON;

namespace {
/* little utility class to make things look nicer in the tests below. */
class url {
private:
   string url_;
   tile_path_parser path_parser;

public:
   url(const string &u) : url_(u) {}
   void should_give(const tile_protocol &tile) const {
      tile_protocol parsed;
      if (!path_parser(parsed, url_)) {
         throw runtime_error(
            (boost::format("Cannot parse URL (%1%) which is expected to be valid.")
             % url_).str());
      }
      if (tile != parsed) {
         throw runtime_error(
            (boost::format("Parsed tile request path is not what was expected (expected %1% but got %2%).")
             % tile % parsed).str());
      }
   }
   void should_be_invalid() const {
      tile_protocol parsed;
      if (path_parser(parsed, url_)) {
         throw runtime_error((boost::format("URL (%1%) was expected to be invalid, but has parsed OK (as %2%).")
                              % url_ % parsed).str());
      }
   }
};

void check_parse_time(const std::string &str, std::time_t expected) 
{
   std::time_t actual;
   if (!rendermq::parse_http_date(actual, str)) 
   {
      throw runtime_error((boost::format("String (%1%) was expected to be a valid time, but has failed to parse.")
                           % str).str());
   }
   if (actual != expected) 
   {
      throw runtime_error((boost::format("Parsed string (%1%) as time %2%, but expected %3%.")
                           % str % actual % expected).str());
   }
}

void check_format_time(const std::string &expected, std::time_t time) 
{
   rendermq::http_date_formatter formatter;
   std::ostringstream ostr;
   formatter(ostr, time);
   if (ostr.str() != expected) {
      throw runtime_error((boost::format("Formatted string (%1%) from time %2%, but expected %3%.")
                           % ostr.str() % time % expected).str());
   }  
}

}

/* check that basic path parsing works, and gets the right format
 * from the extension at the end.
 */
void test_path_parsing_format() {
   url("/tiles/1.0.0/osm/0/0/0.png") .should_give(tile_protocol(cmdRender, 0, 0, 0, 0, "osm", fmtPNG));
   url("/tiles/1.0.0/osm/0/0/0.jpg") .should_give(tile_protocol(cmdRender, 0, 0, 0, 0, "osm", fmtJPEG));
   url("/tiles/1.0.0/osm/0/0/0.jpeg").should_give(tile_protocol(cmdRender, 0, 0, 0, 0, "osm", fmtJPEG));
   url("/tiles/1.0.0/osm/0/0/0.json").should_give(tile_protocol(cmdRender, 0, 0, 0, 0, "osm", fmtJSON));
}

/* check that the path parsing gives the right command appended to
 * the end, and works over multple formats.
 */
void test_path_parsing_command() {
   url("/tiles/1.0.0/osm/0/0/0.png/dirty") .should_give(tile_protocol(cmdDirty,  0, 0, 0, 0, "osm", fmtPNG));
   url("/tiles/1.0.0/osm/0/0/0.jpg/dirty") .should_give(tile_protocol(cmdDirty,  0, 0, 0, 0, "osm", fmtJPEG));
   url("/tiles/1.0.0/osm/0/0/0.jpg/status").should_give(tile_protocol(cmdStatus, 0, 0, 0, 0, "osm", fmtJPEG));
   url("/tiles/1.0.0/foo/0/0/0.png/dirty") .should_give(tile_protocol(cmdDirty,  0, 0, 0, 0, "foo", fmtPNG));
   url("/tiles/1.0.0/bar/0/0/0.jpg/dirty") .should_give(tile_protocol(cmdDirty,  0, 0, 0, 0, "bar", fmtJPEG));
   url("/tiles/1.0.0/baz/0/0/0.jpg/status").should_give(tile_protocol(cmdStatus, 0, 0, 0, 0, "baz", fmtJPEG));
}

/* check that the path parsing gives the right coordinates.
 */
void test_path_parsing_coords() {
   url("/tiles/1.0.0/osm/1/2/3.png")    .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "osm", fmtPNG));
   url("/tiles/1.0.0/osm/4/5/6.jpg")    .should_give(tile_protocol(cmdRender, 5, 6, 4, 0, "osm", fmtJPEG));
   url("/tiles/1.0.0/osm/10/30/50.jpeg").should_give(tile_protocol(cmdRender, 30, 50, 10, 0, "osm", fmtJPEG));
   url("/tiles/1.0.0/osm/15/40/70.json").should_give(tile_protocol(cmdRender, 40, 70, 15, 0, "osm", fmtJSON));
}


/* check that "version" parts of the URL are included as well in
 * the style name which comes out from the parser.
 */
void test_path_parsing_version() {
   url("/tiles/1.0.0/vx/osm/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vx/osm", fmtPNG));
   url("/tiles/1.0.0/vy/osm/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vy/osm", fmtPNG));

   url("/tiles/1.0.0/vx/map/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vx/map", fmtPNG));
   url("/tiles/1.0.0/vy/map/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vy/map", fmtPNG));

   url("/tiles/1.0.0/vx/hyb/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vx/hyb", fmtPNG));
   url("/tiles/1.0.0/vy/hyb/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vy/hyb", fmtPNG));

   url("/tiles/1.0.0/vx/sat/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vx/sat", fmtPNG));
   url("/tiles/1.0.0/vy/sat/1/2/3.png") .should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vy/sat", fmtPNG));
   url("/tiles/1.0.0/vx/other/osm/1/2/3.png").should_give(tile_protocol(cmdRender, 2, 3, 1, 0, "vx/other/osm", fmtPNG));

   url("/tiles/1.0.0/vy/map/13/2353/3085.png").should_give(tile_protocol(cmdRender, 2353, 3085, 13, 0, "vy/map", fmtPNG));
}

/* this is parsing, not checking, so these should parse even though
 * they are invalid coordindates. in other words: these are valid 
 * _paths_ even if they are invalid _tiles_, just like having an
 * unknown style makes it an invalid tile, having an invalid coord
 * doesn't make it an invalid path.
 */
void test_path_parsing_not_checking() {
   url("/tiles/1.0.0/map/1/10/100.png").should_give(tile_protocol(cmdRender, 10, 100, 1, 0, "map", fmtPNG));
   url("/tiles/1.0.0/map/100/1/1.png") .should_give(tile_protocol(cmdRender, 1, 1, 100,  0, "map", fmtPNG));
}

/* check that things which are invalid tile paths do not get parsed.
 */
void test_path_parsing_invalid() {
   url("").should_be_invalid();
   url("/0/0/0.png").should_be_invalid();
   url("///0/0/0.png").should_be_invalid();
   
   // unsupported format
   url("/tiles/1.0.0/osm/0/0/0.tga").should_be_invalid();

   // coords aren't integers
   url("/tiles/1.0.0/osm/x/0/0.png").should_be_invalid();
   url("/tiles/1.0.0/osm/0/y/0.png").should_be_invalid();
   url("/tiles/1.0.0/osm/0/0/z.png").should_be_invalid();

   // integer is too large to fit in an int
   url("/tiles/1.0.0/osm/0/0/123456789012345678901234567890.png").should_be_invalid();

   // unknown command
   url("/tiles/1.0.0/osm/0/0/0.png/killallhumans").should_be_invalid();

   // shouldn't allow spaces anywhere
   url("/tiles/1.0.0/osm/ 0/ 0/ 0.png").should_be_invalid();

   // can't start style names with a digit anywhere
   url("/tiles/1.0.0/1osm/0/0/0.png").should_be_invalid();
   url("/tiles/1.0.0/1osm/map/0/0/0.png").should_be_invalid();
   url("/tiles/1.0.0/osm/1map/0/0/0.png").should_be_invalid();

   // style names can't have non-alphanumeric characters in them
   url("/tiles/1.0.0/osm_foo/0/0/0.png").should_be_invalid();
}

void test_date_parsing() 
{
   // should be, according to RFC 2616, either RFC 1123, RFCs 850/1036
   // or the ANSI C asctime() format.
   check_parse_time("Sun, 06 Nov 1994 08:49:37 GMT", 784111777);
   check_parse_time("Sunday, 06-Nov-94 08:49:37 GMT", 784111777);
   check_parse_time("Sun Nov  6 08:49:37 1994", 784111777);

   check_parse_time("Thu, 01 Jan 1970 00:00:00 GMT", 0);
   check_parse_time("Thursday, 01-Jan-70 00:00:00 GMT", 0);
   check_parse_time("Thu Jan  1 00:00:00 1970", 0);

   check_parse_time("Fri, 03 Jun 2011 13:42:17 GMT", 1307108537);
   check_parse_time("Friday, 03-Jun-11 13:42:17 GMT", 1307108537);
   check_parse_time("Fri Jun  3 13:42:17 2011", 1307108537);
}

void test_date_formatting()
{
   // RFC 2616 says only ever produce RFC 1123 dates.
   check_format_time("Sun, 06 Nov 1994 08:49:37 GMT", 784111777);
   check_format_time("Thu, 01 Jan 1970 00:00:00 GMT", 0);
   check_format_time("Fri, 03 Jun 2011 13:42:17 GMT", 1307108537);
}

int main() {
   int tests_failed = 0;

   cout << "== Testing Handler Functions ==" << endl << endl;

   tests_failed += test::run("test_path_parsing_format", &test_path_parsing_format);
   tests_failed += test::run("test_path_parsing_command", &test_path_parsing_command);
   tests_failed += test::run("test_path_parsing_coords", &test_path_parsing_coords);
   tests_failed += test::run("test_path_parsing_not_checking", &test_path_parsing_not_checking);
   tests_failed += test::run("test_path_parsing_invalid", &test_path_parsing_invalid);
   tests_failed += test::run("test_path_parsing_version", &test_path_parsing_version);
   tests_failed += test::run("test_date_parsing", &test_date_parsing);
   tests_failed += test::run("test_date_formatting", &test_date_formatting);
   //tests_failed += test::run("test_", &test_);

   cout << " >> Tests failed: " << tests_failed << endl << endl;

   return 0;
}
