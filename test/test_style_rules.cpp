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

#include "tile_handler.hpp"
#include "test/common.hpp"

#include <stdexcept>
#include <iostream>
#include <boost/format.hpp>

using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using rendermq::style_rules;
using rendermq::tile_protocol;
namespace pt = boost::property_tree;

using rendermq::cmdRender;
using rendermq::fmtPNG;
using rendermq::fmtGIF;
using rendermq::fmtJPEG;

namespace 
{

void check_is_ok(const tile_protocol &job, const style_rules &rules)
{
   tile_protocol mut_job(job);
   if (!rules.rewrite_and_check(mut_job))
   {
      throw runtime_error((boost::format("Job %1% should have passed the style rules, but did not.") % job).str());
   }
}

void check_is_bad(const tile_protocol &job, const style_rules &rules)
{
   tile_protocol mut_job(job);
   if (rules.rewrite_and_check(mut_job))
   {
      throw runtime_error((boost::format("Job %1% should not have passed the style rules, but did.") % job).str());
   }
}

void check_is_same(const tile_protocol &job, const style_rules &rules)
{
   tile_protocol mut_job(job);
   if (!rules.rewrite_and_check(mut_job))
   {
      throw runtime_error((boost::format("Job %1% should have passed the style rules, but did not.") % job).str());
   }
   if (!(mut_job == job))
   {
      throw runtime_error((boost::format("Job %1% should not have been rewritten, but is now %2%.") % job % mut_job).str());
   }
}

void check_rewrite(const tile_protocol &old_job,
                   const tile_protocol &new_job,
                   const style_rules &rules)
{
   tile_protocol mut_job(old_job);
   if (!rules.rewrite_and_check(mut_job))
   {
      throw runtime_error((boost::format("Job %1% should have passed the style rules, but did not.") % old_job).str());
   }
   if (!(mut_job == new_job))
   {
      if (mut_job == old_job)
      {
         throw runtime_error((boost::format("Job %1% was not re-written, but was expected to be %2%.") % old_job % new_job).str());
      } 
      else
      {
         throw runtime_error((boost::format("Job %1% should have been rewritten as %2%, but is now %3%.") % old_job % new_job % mut_job).str());
      }
   }
}

void test_null()
{
   // empty conf implies the style rules shouldn't change anything.
   pt::ptree conf;
   style_rules rules(conf);

   check_is_same(tile_protocol(), rules);
   check_is_same(tile_protocol(cmdRender, 10, 10, 4, 0, "map", fmtPNG), rules);
   check_is_same(tile_protocol(cmdRender, 10, 20, 5, 0, "map", fmtJPEG), rules);
   check_is_same(tile_protocol(cmdRender, 20, 10, 5, 0, "hyb", fmtPNG), rules);
   check_is_same(tile_protocol(cmdRender, 30, 30, 6, 0, "hyb", fmtJPEG), rules);
   check_is_same(tile_protocol(cmdRender, 10, 10, 4, 0, "map", fmtGIF), rules);
}

// test that some style names can be rewritten to provide "fake"
// styles identical to existing ones, but without having to have
// an actual style with real cache, etc...
void test_rewrite()
{
   pt::ptree conf;
   conf.put("rewrite.map","osm");
   conf.put("rewrite.vx/osm", "osm");
   conf.put("rewrite.vy/osm", "osm");
   conf.put("rewrite.vy/map", "map");
   style_rules rules(conf);
   
   check_rewrite(tile_protocol(cmdRender, 10, 10, 4, 0, "map", fmtPNG), 
                 tile_protocol(cmdRender, 10, 10, 4, 0, "osm", fmtPNG), 
                 rules);
   check_rewrite(tile_protocol(cmdRender, 10, 20, 5, 0, "map", fmtJPEG), 
                 tile_protocol(cmdRender, 10, 20, 5, 0, "osm", fmtJPEG), 
                 rules);
   check_is_same(tile_protocol(cmdRender, 20, 10, 5, 0, "hyb", fmtPNG), rules);
   check_is_same(tile_protocol(cmdRender, 30, 30, 6, 0, "hyb", fmtJPEG), rules);
   check_rewrite(tile_protocol(cmdRender, 10, 10, 4, 0, "map", fmtGIF), 
                 tile_protocol(cmdRender, 10, 10, 4, 0, "osm", fmtGIF), 
                 rules);

   // check the rewrite rule works with versions.
   check_rewrite(tile_protocol(cmdRender, 10, 10, 4, 0, "vx/osm", fmtGIF), 
                 tile_protocol(cmdRender, 10, 10, 4, 0, "osm", fmtGIF), 
                 rules);
   check_rewrite(tile_protocol(cmdRender, 23, 12, 5, 0, "vy/osm", fmtJPEG), 
                 tile_protocol(cmdRender, 23, 12, 5, 0, "osm", fmtJPEG), 
                 rules);   
   check_rewrite(tile_protocol(cmdRender, 2353, 3085, 13, 0, "vy/map", fmtPNG), 
                 tile_protocol(cmdRender, 2353, 3085, 13, 0, "map", fmtPNG), 
                 rules);   
}

// test that the formats for a particular style are re-written, 
// but only for that style.
void test_force_format()
{
   pt::ptree conf;
   conf.put("forced_formats.map","jpeg");
   conf.put("forced_formats.hyb","gif");
   style_rules rules(conf);
   
   check_rewrite(tile_protocol(cmdRender, 10, 10, 4, 0, "map", fmtPNG), 
                 tile_protocol(cmdRender, 10, 10, 4, 0, "map", fmtJPEG), 
                 rules);
   check_is_same(tile_protocol(cmdRender, 10, 20, 5, 0, "map", fmtJPEG), rules);
   check_rewrite(tile_protocol(cmdRender, 20, 10, 5, 0, "hyb", fmtPNG), 
                 tile_protocol(cmdRender, 20, 10, 5, 0, "hyb", fmtGIF), 
                 rules);
   check_is_same(tile_protocol(cmdRender, 30, 30, 6, 0, "hyb", fmtGIF), rules);
   check_is_same(tile_protocol(cmdRender, 10, 10, 4, 0, "osm", fmtPNG), rules); 
   check_rewrite(tile_protocol(cmdRender, 2353, 3085, 13, 0, "map", fmtPNG), 
                 tile_protocol(cmdRender, 2353, 3085, 13, 0, "map", fmtJPEG), 
                 rules);   
}

// test that invalid zoom levels, and tiles outside the zoom
// range, are checked for on a per-style basis.
void test_zoom_levels()
{
   pt::ptree conf;
   conf.put("zoom_limits.map", 19);
   style_rules rules(conf);

   // some basic checks
   check_is_ok (tile_protocol(cmdRender,  1,  1,  1, 0, "map", fmtPNG), rules);
   check_is_bad(tile_protocol(cmdRender,  0,  0, -1, 0, "map", fmtPNG), rules);
   check_is_bad(tile_protocol(cmdRender, -1, -1,  1, 0, "map", fmtPNG), rules);
   check_is_bad(tile_protocol(cmdRender,  3,  3,  1, 0, "map", fmtPNG), rules);

   // check z18 and z19 are ok, but z20 isn't for map style
   check_is_ok (tile_protocol(cmdRender,  1,  1, 18, 0, "map", fmtPNG), rules);
   check_is_ok (tile_protocol(cmdRender,  1,  1, 19, 0, "map", fmtPNG), rules);
   check_is_bad(tile_protocol(cmdRender,  1,  1, 20, 0, "map", fmtPNG), rules);

   // check z18 is ok, but z19 & z20 aren't for any other style
   check_is_ok (tile_protocol(cmdRender,  1,  1, 18, 0, "osm", fmtPNG), rules);
   check_is_bad(tile_protocol(cmdRender,  1,  1, 19, 0, "osm", fmtPNG), rules);
   check_is_bad(tile_protocol(cmdRender,  1,  1, 20, 0, "osm", fmtPNG), rules);

   check_is_ok(tile_protocol(cmdRender, 2353, 3085, 13, 0, "map", fmtJPEG), rules);
}

} // anonymous namespace

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Handler Tile Rewrite Rules ==" << endl << endl;
   
   tests_failed += test::run("test_null", &test_null);
   tests_failed += test::run("test_rewrite", &test_rewrite);
   tests_failed += test::run("test_force_format", &test_force_format);
   tests_failed += test::run("test_zoom_levels", &test_zoom_levels);
   //tests_failed += test::run("test_", &test_);
   
   cout << " >> Tests failed: " << tests_failed << endl << endl;
   
   return 0;
}
