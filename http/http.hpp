/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq
 *
 *  Author: matt.amos@mapquest.com
 *  Author: kevin.kreiser@mapquest.com
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

#ifndef HTTP_HPP
#define HTTP_HPP

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>
#include <boost/shared_ptr.hpp>

namespace http
{
typedef boost::shared_ptr<CURL> curl_ptr;
typedef std::vector<std::string> headers_t;

   //a means of keeping the response from the server
   struct response
   {
         response(const long& statusCode = -1, const long& timeStamp = -1): statusCode(statusCode), timeStamp(timeStamp){}
         long statusCode;
         long timeStamp;
         headers_t headers;
         std::string body;
   };

   //a means of specifying multipart post data
   struct part
   {
         part(const char* data, const long& size):data(data),name(""),fileName(NULL),mime(""),size(size),position(0){}
         part(const char* data, const long& size, const char* name, const char* filename, const char* mime, const long& position=0):data(data),name(name),fileName(filename),mime(mime),size(size),position(position){}
         mutable char const* data;
         mutable char const* name;
         mutable char const* fileName;
         mutable char const* mime;
         long size;
         long position;

   };

   // create a curl handle to use as a persistent connection
   curl_ptr createPersistentConnection();

// perform HTTP get, returning the response
boost::shared_ptr<response> get(
   const std::string &url,
   curl_ptr connection = curl_ptr(), 
   const headers_t& headers = headers_t(), 
   const bool& keepHeaders = false, 
   long timeout = 0L);

   // perform HTTP post, returning the response
boost::shared_ptr<response> post(
   const std::string &url, 
   const std::string &post,
   curl_ptr connection = curl_ptr(), 
   const headers_t& headers = headers_t(), 
   const bool& keepHeaders = false);

// perform HTTP post of multipart form, returning the response
boost::shared_ptr<response> postForm(
   const std::string &url, 
   const std::vector<part>& parts,
   curl_ptr connection = curl_ptr(), 
   const headers_t& headers = headers_t(), 
   const bool& keepHeaders = false, 
   const bool& submit = false);

// perform HTTP multi-post, returning the responses in the same order as the requests.
// each request is a pair<string, string> of the URL and the data to post.
std::vector<boost::shared_ptr<response> > multiPost(
   const std::vector<std::pair<std::string, std::string> > &requests,
   size_t concurrency,
   curl_ptr connection = curl_ptr(),
   const headers_t &headers = headers_t(),
   const bool &keepHeaders = false);

// perform HTTP multi-post with forms, returning the responses in the same order 
// as the requests. each request is a pair<string, vector<part>> of the URL and 
// the form parts to post.
std::vector<boost::shared_ptr<response> > multiPostForm(
   const std::vector<std::pair<std::string, std::vector<part> > > &requests,
   size_t concurrency,
   curl_ptr connection = curl_ptr(),
   const headers_t &headers = headers_t(),
   const bool &keepHeaders = false,
   const bool &submit = false);

// perform HTTP multi-get, returning responses in the same order as requests.
std::vector<boost::shared_ptr<response> > multiGet(
   const headers_t &urls,
   size_t concurrency,
   curl_ptr connection = curl_ptr(),
   const headers_t &headers = headers_t(),
   const bool &keepHeaders = false);

   // perform HTTP delete, returning the response
   boost::shared_ptr<response> del(const std::string &url,
      curl_ptr connection = curl_ptr(), const headers_t& headers = headers_t(), const bool& keepHeaders = false);

   // perform HTTP head, returning the response
   boost::shared_ptr<response> head(const std::string &url,
      curl_ptr connection = curl_ptr(), const headers_t& headers = headers_t(), const bool& keepHeaders = false);

   // escape a string into "URL-encoded" RFC2396 compliant string
   std::string escape_url(const std::string &url,
      curl_ptr connection = curl_ptr());

   // unescape a string from a "URL-encoded" RFC2396 compliant string
   std::string unescape_url(const std::string &url,
      curl_ptr connection = curl_ptr());

}

#endif /* HTTP_HPP */
