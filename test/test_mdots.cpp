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

#include "milliondots/path_parser.hpp"
#include "milliondots/location.hpp"
#include "milliondots/query.hpp"
#include "milliondots/state.hpp"
#include "image/image.hpp"
#include "test/common.hpp"

#include <vector>
#include <string>
#include <stdio.h>
#include <stdexcept>
#include <iostream>
#include <boost/format.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <cmath>

using std::runtime_error;
using std::exception;
using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::list;
using std::vector;
using rendermq::milliondots::path_parser;
using rendermq::milliondots::request;
using rendermq::milliondots::query;
using rendermq::milliondots::location;
using rendermq::milliondots::state;
using rendermq::mongrel_request;
using boost::shared_ptr;
using boost::make_shared;

namespace 
{

void assert_parses(const string &s) 
{
   path_parser parser;

   shared_ptr<request> actual = make_shared<request>("fake_id", "fake_uuid");
   
   bool result = parser(actual->m_request_path, s);
   if (!result)
   {
      throw std::runtime_error((boost::format("String `%1%' failed to parse, but should have.") % s).str());
   }
   /*
   if (!(expected == actual))
   {
      throw std::runtime_error((boost::format("String `%1%' parsed as %2% but expected %3%.") % s % actual % expected).str());
   }
   */
}

// error limit on projections - a ten thousandth of a pixel seems
// like a reasonable error limit, i.e: much less than could be
// detected in the rendered image.
#define PROJ_EPS (1.0e-4)

// error limit on inverse (i.e: to lon/lat) projections. this needs
// to be much tighter, as the numeric space is relatively much 
// smaller. note that this is pre-scaled by the scale of the 
// projection first, as larger scales always have larger errors.
#define PROJ_INV_EPS (1.0e-12)

void assert_projects(int tx, int ty, float scale, location::proj proj, 
                     double lon, double lat,
                     double expected_x, double expected_y)
{
   location loc;
   loc.x = tx;
   loc.y = ty;
   loc.scale = scale;
   loc.projection = proj;

   double x = lon, y = lat;
   bool ok = loc.forward(x, y);
   if (!ok) 
   {
      throw runtime_error((boost::format("At zoom %1%, tile (%2%, %3%) point (%4%, %5%), "
                                         "projection failed!") 
                           % scale % tx % ty % lon % lat).str());
   }
   
   if ((std::abs(x - expected_x) > PROJ_EPS) || 
       (std::abs(y - expected_y) > PROJ_EPS)) 
   {
      throw runtime_error((boost::format("At zoom %1%, tile (%2%, %3%): expected (%4%, %5%), "
                                         "instead got (%6%, %7%)") 
                           % scale % tx % ty % expected_x % expected_y % x % y).str());
   }
}

void assert_unprojects(int tx, int ty, float scale, location::proj proj, 
                       double x, double y,
                       double expected_lon, double expected_lat)
{
   location loc;
   loc.x = tx;
   loc.y = ty;
   loc.scale = scale;
   loc.projection = proj;

   double lon = x, lat = y;
   bool ok = loc.inverse(lon, lat);
   if (!ok) 
   {
      throw runtime_error((boost::format("At zoom %1%, tile (%2%, %3%) point (%4%, %5%), "
                                         "projection failed!") 
                           % scale % tx % ty % x % y).str());
   }

   // scale correction for spherical mercator to get the error
   // approximately right.
   if (proj == location::proj_sm)
   {
      scale = (1 << int(24 - scale));
   }

   if ((std::abs(lon - expected_lon) / scale > PROJ_INV_EPS) || 
       (std::abs(lat - expected_lat) / scale > PROJ_INV_EPS)) 
   {
      throw runtime_error((boost::format("At zoom %1%, tile (%2%, %3%): expected (%4%, %5%), "
                                         "instead got (%6%, %7%) [%8%, %9%]") 
                           % scale % tx % ty % expected_lon % expected_lat % lon % lat
                           % std::abs(lon - expected_lon) % std::abs(lat - expected_lat)).str());
   }
}

void test_query_grammar()
{
   // default icon
   assert_parses("/layer/search/food/tile");

   // dot icon with defaults
   assert_parses("/layer/search/food[rgb(1,2,3)]/tile");
   assert_parses("/layer/search/food[rgb(1,2,3):255]/tile");
   assert_parses("/layer/search/food[rgb(1,2,3):255:rgb(3,2,1)]/tile");
   assert_parses("/layer/search/food[rgb(1,2,3):255:rgb(3,2,1):10]/tile");
   assert_parses("/layer/search/food[rgb(1,2,3):255:rgb(3,2,1):10:10]/tile");
   assert_parses("/layer/search/food[rgb(1,2,3):255:rgb(3,2,1):10:10:10]/tile");

   // static icon
   assert_parses("/layer/search/food[static(foobar)]/tile");

   // multiple queries
   assert_parses("/layer/search/food;bar/data");
   assert_parses("/layer/search/food;bar[rgb(1,2,3)]/tile");
   assert_parses("/layer/search/food[rgb(1,2,3)];bar[rgb(1,2,3)]/data");
}

void test_sm_projection()
{
   // assert the origin maps to the upper-left corner of the
   // central tile for all zooms except 0.
   for (int z = 1; z < 18; ++z) 
   {
      int tt = (1 << (z - 1));
      assert_projects(tt, tt, z, location::proj_sm, 0.0, 0.0, 0.0, 0.0);
   }
   // zoom zero is a special case - it is only a single tile, so 
   // the origin maps to a pixel in the middle of it.
   assert_projects(0, 0, 0, location::proj_sm, 0.0, 0.0, 128.0, 128.0);

   // check that the upper left point in space maps to the zero
   // tile, zero pixel.
   for (int z = 0; z < 19; ++z) 
   {
      assert_projects(0, 0, z, location::proj_sm, -180.0, 85.0511287798, 0.0, 0.0);
   }

   // check that the lower right point in space maps to the final
   // tile, 256th pixel.
   for (int z = 0; z < 19; ++z) 
   {
      int tt = (1 << z) - 1;
      assert_projects(tt, tt, z, location::proj_sm, 180.0, -85.0511287798, 256.0, 256.0);
   }

   // check the projections around a point
   assert_projects(32768,   32768, 16, location::proj_sm, 0.0, 0.0,   0.0,   0.0);
   assert_projects(32767,   32768, 16, location::proj_sm, 0.0, 0.0, 256.0,   0.0);
   assert_projects(32768,   32767, 16, location::proj_sm, 0.0, 0.0,   0.0, 256.0);
   assert_projects(32767,   32767, 16, location::proj_sm, 0.0, 0.0, 256.0, 256.0);
}

void test_mq_projection()
{
   // at all scales, the 0,0 tile is in the lower right (-180,-90) 
   // corner of the world. note that the y is at 256 because display
   // coordinates have a reversed y direction from geographic coords.
   for (int i = 1; i <= location::max_mq_scale; ++i)
   {
      assert_projects(0, 0, location::mq_scales[i], location::proj_mq, -180, -90, 0.0, 256.0);
   }

   // these tiles are near the middle of the world
   assert_projects(2, 1, 88011773, location::proj_mq, -2.5662962988496077e-06, -18.598200391778718, 0.0, 256.0);
   assert_projects(2, 1, 88011773, location::proj_mq, 89.99999615055556, 52.803599216442578, 256.0, 0.0);
   assert_projects(19558, 12326, 9000, location::proj_mq, -0.0015834911348039918, -0.0019580591971447652, 0.0, 256.0);
   
   assert_projects(117349,73957, 1500, location::proj_mq, -4.9605457074717576e-05, -0.00074114587110221523, 0.0, 256.0);
   assert_projects(176023,110936, 1000, location::proj_mq, -0.00056090068298075553, -0.00033550809574879554, 0.0, 256.0);

   // test that tile boundaries are correct / as we expect them to be
   assert_projects(176023,110936, 1000, location::proj_mq, -0.00056090068298075553, -0.00033550809574879554, 0.0, 256.0);
   assert_projects(176022,110936, 1000, location::proj_mq, -0.00056090068298075553, -0.00033550809574879554, 256.0, 256.0);
   assert_projects(176022,110935, 1000, location::proj_mq, -0.00056090068298075553, -0.00033550809574879554, 256.0, 0.0);
   assert_projects(176023,110935, 1000, location::proj_mq, -0.00056090068298075553, -0.00033550809574879554, 0.0, 0.0);

}

void test_sm_inv_projection()
{
   // assert the the upper-left corner of the central tile maps
   // to the origin for all zooms except 0.
   for (int z = 1; z < 18; ++z) 
   {
      int tt = (1 << (z - 1));
      assert_unprojects(tt, tt, z, location::proj_sm, 0.0, 0.0, 0.0, 0.0);
   }
   // zoom zero is a special case - it is only a single tile, so 
   // the origin maps to a pixel in the middle of it.
   assert_unprojects(0, 0, 0, location::proj_sm, 128.0, 128.0, 0.0, 0.0);

   // check that the upper left point in space maps to the zero
   // tile, zero pixel.
   for (int z = 0; z < 19; ++z) 
   {
      assert_unprojects(0, 0, z, location::proj_sm, 0.0, 0.0, -180.0, 85.0511287798);
   }

   // check that the lower right point in space maps to the final
   // tile, 256th pixel.
   for (int z = 0; z < 19; ++z) 
   {
      int tt = (1 << z) - 1;
      assert_unprojects(tt, tt, z, location::proj_sm, 256.0, 256.0, 180.0, -85.0511287798);
   }

   // check the projections around a point
   assert_unprojects(32768,   32768, 16, location::proj_sm,   0.0,   0.0, 0.0, 0.0);
   assert_unprojects(32767,   32768, 16, location::proj_sm, 256.0,   0.0, 0.0, 0.0);
   assert_unprojects(32768,   32767, 16, location::proj_sm,   0.0, 256.0, 0.0, 0.0);
   assert_unprojects(32767,   32767, 16, location::proj_sm, 256.0, 256.0, 0.0, 0.0);
}

void test_mq_inv_projection()
{
   // at all scales, the 0,0 tile is in the lower right (-180,-90) 
   // corner of the world. note that the y is at 256 because display
   // coordinates have a reversed y direction from geographic coords.
   for (int i = 1; i <= location::max_mq_scale; ++i)
   {
      assert_unprojects(0, 0, location::mq_scales[i], location::proj_mq, 0.0, 256.0, -180, -90);
   }

   // these tiles are near the middle of the world
   assert_unprojects(2, 1, 88011773, location::proj_mq, 0.0, 256.0, -2.5662962988496077e-06, -18.598200391778718);
   assert_unprojects(2, 1, 88011773, location::proj_mq, 256.0, 0.0, 89.99999615055556, 52.803599216442578);
   assert_unprojects(19558, 12326, 9000, location::proj_mq, 0.0, 256.0, -0.0015834911348039918, -0.0019580591971447652);
   
   assert_unprojects(117349,73957, 1500, location::proj_mq, 0.0, 256.0, -4.9605457074717576e-05, -0.00074114587110221523);
   assert_unprojects(176023,110936, 1000, location::proj_mq, 0.0, 256.0, -0.00056090068298075553, -0.00033550809574879554);

   // test that tile boundaries are correct / as we expect them to be
   assert_unprojects(176023,110936, 1000, location::proj_mq, 0.0, 256.0, -0.00056090068298075553, -0.00033550809574879554);
   assert_unprojects(176022,110936, 1000, location::proj_mq, 256.0, 256.0, -0.00056090068298075553, -0.00033550809574879554);
   assert_unprojects(176022,110935, 1000, location::proj_mq, 256.0, 0.0, -0.00056090068298075553, -0.00033550809574879554);
   assert_unprojects(176023,110935, 1000, location::proj_mq, 0.0, 0.0, -0.00056090068298075553, -0.00033550809574879554);

}

void test_gd_draw()
{
   vector<std::pair<int,int> > dots;
   int dim = 256;
   int dotSize = 4;
   int outline = 4;
   int both = dotSize + outline;
   tuple<int,int,int,int> dotColor(255, 0, 0, 255);
   tuple<int,int,int,int> outlineColor(0, 255, 0, 255);
   tuple<int,int,int,int> backgroundColor(0, 0, 0, 0);

   //make some dots along the diagonal
   for(int i = both; i < dim - both; i += both)
      dots.push_back(std::make_pair(i, i));

   //throw some outside the edges
   dots.push_back(std::make_pair(dim, -dotSize/2));
   dots.push_back(std::make_pair(-dotSize/2, dim));

   //make an image
   boost::shared_ptr<rendermq::image> img = rendermq::image::createMillionDotsImage(dots, dim, dim, dotColor, outlineColor, backgroundColor, dotSize, outline);

   //somehow image failed to be created
   if(img == NULL)
      throw runtime_error("No image rendered");

   //get the data
   string data = img->save(rendermq::fmtPNG);
   if(data.length() < 1)
      throw runtime_error("Incorrect image data");

   //save the image
   FILE* file = fopen("tmp.png", "wb");
   fwrite(data.c_str(), 1, data.length(), file);
   fclose(file);
}

void test_xml_parsing()
{
   // this is taken from a real GSS result
   static const char *xml_str = 
      "<?xml version=\"1.0\" encoding=\"UTF-8\"  ?><SearchResults><MetaData><originalQuery>"
      "cat:live theaters</originalQuery><displayQuery>cat:live theaters</displayQuery><"
      "recommendedMapBestFitIndex>0</recommendedMapBestFitIndex><resultRelevancyCutoffI"
      "ndex>0</resultRelevancyCutoffIndex><searchTimeMS>3</searchTimeMS><totalAvailable"
      "Results>2</totalAvailableResults><searchType>map</searchType><sortBy>relevance</"
      "sortBy><numResultsReturned>2</numResultsReturned><infer>none</infer><clip>force-"
      "boundingbox</clip><searchLatLng>(40.7327,-73.9847)</searchLatLng><searchRadius/>"
      "<searchBoundingbox>( (40.7349,-73.9876) ,  (40.7304,-73.9817))</searchBoundingbo"
      "x><searchPolygon/><sourcevendorFilter/><gasPriceType></gasPriceType><street></st"
      "reet><city></city><state></state><country></country><postalCode></postalCode><ge"
      "ocodeQuality></geocodeQuality><searchRouteLength></searchRouteLength><fullRouteL"
      "ength></fullRouteLength><QueryAnalysis><requestType>user</requestType><queryType"
      ">category</queryType><spatialRelevance>scale:16(1000)</spatialRelevance><inverse"
      "TermFrequency>77</inverseTermFrequency><highHit>0</highHit><latch>1</latch><latc"
      "hQuery>sic:792207</latchQuery><textMatchRatio>0.0</textMatchRatio><poiDensity>16"
      "</poiDensity><searchResultQuality>100</searchResultQuality><codes><code>792207</"
      "code></codes></QueryAnalysis></MetaData><SearchResult><rank>1</rank><id>1108113<"
      "/id><score>20.818226</score><distance>0.100371085</distance><fields><name>Smuggl"
      "ers Cove Entertainment</name><lat>40.731257</lat><lng>-73.984485</lng><displayLa"
      "t></displayLat><displayLng></displayLng><geomatchcode>L1</geomatchcode></fields>"
      "</SearchResult><SearchResult><rank>2</rank><id>1107850</id><score>20.818226</sco"
      "re><distance>0.13820986</distance><fields><name>Village East Theatres Inc</name>"
      "<lat>40.730944</lat><lng>-73.985964</lng><displayLat></displayLat><displayLng></"
      "displayLng><geomatchcode>L1</geomatchcode></fields></SearchResult></SearchResult"
      "s>";

   // set up same location as the original request
   request::request_path rp;
   location loc;
   path_parser parser;
   int parent_counter = 1;

   // try and parse the strings...
   if (!parser(rp, "/layer/search/cat:live%20theaters[rgb(255,90,0):255:rgb(0,0,0):1:120:7]/data"))
   {
      throw runtime_error("Expected request path to parse, but it didn't.");
   }
   if (!query_parse(loc, "s=16&y=24635&x=19299&p=sm"))
   {
      throw runtime_error("Expected request query parameters to parse, but they didn't");
   }
   if (rp.m_queries.size() != 1)
   {
      throw runtime_error("Expected one query, but got some other number.");
   }

   state st(*rp.m_queries.begin(), loc, parent_counter,
            "http://fake.location/", 0, 0, 0);

   // make a mutable copy of the xml string
   size_t xml_len = strlen(xml_str);
   char *xml_str_cpy = strndup(xml_str, xml_len);

   // try reading the data...
   size_t ret = state::read_data(xml_str_cpy, 1, xml_len, (void *)(&st));
   free(xml_str_cpy);
   if (ret != xml_len)
   {
      throw runtime_error("Expected state::read_data to return ok, but it failed.");
   }

   if (!st.finish())
   {
      throw runtime_error("Expected state::finish to return ok, but it failed.");
   }

   if (st.is_error())
   {
      throw runtime_error("Expected state to be good after parse, but it has flagged an error.");
   }

   if (st.m_pois.size() != 2)
   {
      throw runtime_error((boost::format("Should have parsed 2 pois, but got %1%") 
                           % st.m_pois.size()).str());
   }

   // check the parsed values
   list<state::poi>::const_iterator itr = st.m_pois.begin();

// define a little pre-processor macro to keep the boilerplate away.
#define CHECK_POI(fld, val) { \
      if (itr-> fld != (val)) \
      { \
         throw runtime_error((boost::format("Expected " #fld " to be %1%, but is %2%") \
                              % (val) % itr-> fld).str()); \
      } }

   CHECK_POI(id, "1108113");
   CHECK_POI(name, "Smugglers Cove Entertainment");
   CHECK_POI(lat, 40.731257);
   CHECK_POI(lon, -73.984485);

   ++itr;
   CHECK_POI(id, "1107850");
   CHECK_POI(name, "Village East Theatres Inc");
   CHECK_POI(lat, 40.730944);
   CHECK_POI(lon, -73.985964);

// don't need this any more
#undef CHECK_POI   
}

} // anonymous namespace

int main() 
{
   int tests_failed = 0;
   
   cout << "== Testing Million Dots ==" << endl << endl;
   
   tests_failed += test::run("test_query_grammar", &test_query_grammar);
   tests_failed += test::run("test_sm_projection", &test_sm_projection);
   tests_failed += test::run("test_sm_inv_projection", &test_sm_inv_projection);
   tests_failed += test::run("test_mq_projection", &test_mq_projection);
   tests_failed += test::run("test_mq_inv_projection", &test_mq_inv_projection);
   tests_failed += test::run("test_gd_draw", &test_gd_draw);
   tests_failed += test::run("test_xml_parsing", &test_xml_parsing);
   //tests_failed += test::run("test_", &test_);
   
   cout << " >> Tests failed: " << tests_failed << endl << endl;
   
   return 0;
}
