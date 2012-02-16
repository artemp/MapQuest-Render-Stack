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

#include "http.hpp"
#include "../logging/logger.hpp"
#include <boost/function.hpp>
#include <boost/variant.hpp>
#include <boost/foreach.hpp>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <stack>
#include <list>

using std::string;
using std::pair;
using std::vector;
using std::stack;
using std::list;
using std::runtime_error;
using std::ostringstream;
using boost::shared_ptr;

// callback functions for CURL reader
namespace
{
   size_t write_callback(void *socketBuffer, size_t byteSize, size_t numBytes, void *object)
   {
      // just append the data to the response body
      size_t length = byteSize * numBytes;
      http::response *resp = (http::response *)object;
      resp->body.append((const char *)socketBuffer, length);
      return length;
   }

   size_t read_callback(void *socketBuffer, size_t byteSize, size_t numBytes, void *object)
   {
      //no more buffer left
      long length = long(byteSize * numBytes);
      if(length < 1)
         return 0;

      //get the post object
      http::part* post = (http::part*)object;
      //if we have more to post
      if(post->position < post->size)
      {
         //how much are we reading
         if(post->position + length > post->size)
            length = post->size - post->position;
         //where are we reading from
         const char* data = post->data + post->position;
         //read the data into the buffer
         memcpy(socketBuffer, data, length);
         //keep track of how far along we are
         post->position += length;
         return length;
      }

      //no more post data
      return 0;
   }

   size_t header_callback(void *socketBuffer, size_t byteSize, size_t numBytes, void *object)
   {
      //just append the data to the response headers
      size_t length = byteSize * numBytes;
      http::response *resp = (http::response *)object;
      resp->headers.push_back(string(""));
      resp->headers.back().append((const char *)socketBuffer, length);

      return length;
   }

   size_t discard_callback(void *socketBuffer, size_t byteSize, size_t numBytes, void *object)
   {
      return byteSize * numBytes;
   }

/*************************************************************
 * curl multi interface
 *
 * in order to share as much code as possible between the
 * various implementations of different cURL operations here, 
 * i've restructured the code so that the operation is 
 * factored out as an object (derived from curl_oper). there 
 * are extensions for a single POST, a form POST and a GET
 * also.
 *
 * to handle the multi stuff there is a templated function
 * do_multi_requests() which takes a functor to deal with 
 * unpacking the request details into an instance of 
 * curl_oper.
 *
 *************************************************************/
using http::response;
using http::part;
using http::createPersistentConnection;

/* response or error variant - in order to not throw exceptions
 * during multi-request sessions, this is used to preserve 
 * error information until all the requests are finished.
 */
typedef boost::variant<shared_ptr<response>, string> response_or_error_t;

// visitor to unpack the variant into the response, or throw
// the error if there was one.
struct response_or_error_visitor : public boost::static_visitor<>
{
   typedef shared_ptr<response> result_type;

   result_type operator()(shared_ptr<response> resp) const
   {
      return resp;
   }

   result_type operator()(const string &str) const
   {
      throw runtime_error(str);
   }
};

/* base class of cURL operation representation objects. this 
 * object lives for as long as the operation and collects all
 * the data that is necessary for performing it. however, it 
 * doesn't perform it, as that's done in a different way 
 * between the "easy" and "multi" ways of doing things.
 */
struct curl_oper : private boost::noncopyable
{
   curl_oper(shared_ptr<CURL> curl_ptr,
             const string &url,
             const vector<string> &headers,
             bool keepHeaders,
             size_t index = 0)
      : m_index(index), m_curl(curl_ptr), m_url(url), 
        m_header_list(NULL), m_resp(new response()),
        m_reset_connection(true)
   {
      if (!m_curl) 
      {
         m_curl = createPersistentConnection();
         m_reset_connection = false;
      }
      CURL *curl = m_curl.get();

      if (curl == NULL) 
      {
         throw runtime_error("Cannot initialise CURL library.");
      }

      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(curl, CURLOPT_URL, m_url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, m_resp.get());
      curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);
      
      //send headers along
      for(vector<string>::const_iterator header = headers.begin(); header != headers.end(); header++)
      {
         m_header_list = curl_slist_append(m_header_list, header->c_str());
         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, m_header_list);
      }
      
      //keep the headers or not
      if(keepHeaders)
         curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
      else
         curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, discard_callback);
      curl_easy_setopt(curl, CURLOPT_HEADERDATA, m_resp.get());
   }

   virtual ~curl_oper() 
   {
      if (m_header_list != NULL)
      {
         curl_slist_free_all(m_header_list);
         m_header_list = NULL;
      }
      if (m_reset_connection)
      {
         curl_easy_reset(m_curl.get());
      }
   }

   // returns the name of this operation type for error reporting
   // purposes.
   virtual string oper_type() const = 0;

   // this function is called, with the status, once an oper has 
   // been completed and lets us handle the error, or unpack info
   // from the curl object after everything is done.
   response_or_error_t finish(CURLcode status)
   {
      if (status == 0) 
      {
         curl_easy_getinfo(m_curl.get(), CURLINFO_RESPONSE_CODE, &m_resp->statusCode);
         curl_easy_getinfo(m_curl.get(), CURLINFO_FILETIME, &m_resp->timeStamp);
         return response_or_error_t(m_resp);
      }
      else
      {
         ostringstream ostr;
         ostr << "CURL error on " << oper_type() << " to `" << m_url << "',:" << curl_easy_strerror(status);
         
         return response_or_error_t(ostr.str());
      }
   }

   // the index (not used for easy/single transfers) of the post in
   // the input vector.
   const size_t m_index;

   // the CURL object/connection.
   shared_ptr<CURL> m_curl;

   // URL of the POST.
   const string m_url; 

private:

   struct curl_slist *m_header_list;
   shared_ptr<response> m_resp;
   
   // whether to reset the connection once the transfer is
   // done. set to false for single connections which aren't
   // shared, true otherwise.
   bool m_reset_connection;
};

/* a single POST operation with a given body. this is in
 * contrast to curl_post_form, which takes a form as its 
 * body.
 */
struct curl_post_single : public curl_oper
{
   curl_post_single(shared_ptr<CURL> curl_ptr,
                    const string &url,
                    const part &data,
                    const vector<string> &headers,
                    bool keepHeaders,
                    size_t index = 0)
      : curl_oper(curl_ptr, url, headers, keepHeaders, index),
        m_data(data)
   {
      curl_easy_setopt(m_curl.get(), CURLOPT_POST, 1L);
      curl_easy_setopt(m_curl.get(), CURLOPT_READFUNCTION, read_callback);
      curl_easy_setopt(m_curl.get(), CURLOPT_READDATA, &m_data);
   }

   string oper_type() const
   {
      return "POST";
   }

private:
   const part m_data; 
};

/* post object representing a multi-part form upload.
 */
struct curl_post_form : public curl_oper
{
   curl_post_form(shared_ptr<CURL> curl_ptr,
                  const string &url,
                  const vector<part> &parts,
                  const vector<string> &headers,
                  bool keepHeaders,
                  bool submit,
                  size_t index = 0)
      : curl_oper(curl_ptr, url, headers, keepHeaders, index),
        m_parts(parts), m_postStart(NULL)
   {
      //add each part of the multi part form
      struct curl_httppost *postEnd = NULL;

      for(size_t i = 0; i < m_parts.size(); i++)
      {
         //get the part
         const part* part = &m_parts[i];
         //if its a file part then we need to send it that way
         if(part->fileName)
         {
            //to trick curl to do a file upload you have to set the fileName
            curl_formadd(&m_postStart, &postEnd, CURLFORM_COPYNAME, part->name,
                         CURLFORM_FILENAME, part->fileName, CURLFORM_STREAM, part,
                         CURLFORM_CONTENTSLENGTH, part->size,
                         CURLFORM_CONTENTTYPE, part->mime, CURLFORM_END);
         }//regular text post
         else
         {
            curl_formadd(&m_postStart, &postEnd, CURLFORM_COPYNAME, part->name,
                         /*CURLFORM_FILENAME, part->fileName,*/ CURLFORM_STREAM, part,
                         CURLFORM_CONTENTSLENGTH, part->size,
                         CURLFORM_CONTENTTYPE, part->mime, CURLFORM_END);
         }
      }
      //some servers want you to have the submit button
      if(submit)
         curl_formadd(&m_postStart, &postEnd, CURLFORM_COPYNAME, "submit", CURLFORM_COPYCONTENTS, "send", CURLFORM_END);

      curl_easy_setopt(m_curl.get(), CURLOPT_READFUNCTION, read_callback);
      curl_easy_setopt(m_curl.get(), CURLOPT_HTTPPOST, m_postStart);
   }

   virtual ~curl_post_form() 
   {
      if (m_postStart != NULL) 
      {
         curl_formfree(m_postStart);
      }
   }

   string oper_type() const
   {
      return "POST";
   }

private:
   const vector<part> m_parts;
   struct curl_httppost *m_postStart;
};

/* curl operation to perform an HTTP GET.
 */
struct curl_get : public curl_oper
{
   curl_get(shared_ptr<CURL> curl_ptr,
            const string &url,
            const vector<string> &headers,
            bool keepHeaders,
            size_t index = 0,
            long timeout = 0L)
      : curl_oper(curl_ptr, url, headers, keepHeaders, index)
   {
      curl_easy_setopt(m_curl.get(), CURLOPT_CONNECTTIMEOUT_MS, timeout);
      curl_easy_setopt(m_curl.get(), CURLOPT_HTTPGET, 1L);
   }

   string oper_type() const
   {
      return "GET";
   }
};

/* template function to abstract the multi-curl stuff across both
 * the single and form types of upload. the second argument is a 
 * functor, used to turn the request type into a representative 
 * object.
 */
template <typename T>
vector<shared_ptr<response> > do_multi_requests(
   const vector<T> &requests,
   boost::function<shared_ptr<curl_oper> (const T &t, shared_ptr<CURL> conn, size_t idx)> mk_request,
   size_t concurrency,
   shared_ptr<CURL> connection)
{
   shared_ptr<CURLM> curl_multi(curl_multi_init(), &curl_multi_cleanup);

   // pre-allocate an array for the responses, which are going to be in
   // the same order as the requests.
   vector<response_or_error_t> responses(requests.size());

   // "free" as in available connections so that we can re-use open HTTP
   // connections as much as possible to reduce TCP setup time.
   stack<shared_ptr<CURL> > free_connections;

   // POST objects which are in progress at the moment.
   list<shared_ptr<curl_oper> > in_progress;

   if (!curl_multi)
   {
      throw runtime_error("Cannot set up the cURL::multi system.");
   }

   // don't start up more connections than are needed
   concurrency = std::min(concurrency, requests.size());

   // setup the required number of connections, re-using the one which was 
   // passed in, if appropriate.
   if (connection)
   {
      free_connections.push(connection);
   }
   while (free_connections.size() < concurrency)
   {
      shared_ptr<CURL> curl = createPersistentConnection();
      if (!curl) 
      {
         throw runtime_error("Cannot set up a cURL connection.");
      }
      free_connections.push(curl);
   }

   // req_i runs over the indices of the request objects in the input
   // vector (and correspondingly over the output responses).
   size_t req_i = 0;

   // keep track of the number of running handles in the curl system
   // so that we know when something has changed between runs.
   size_t running_handles = 0;

   while (true)
   {
      // check if there's available jobs and free connections and add
      // handles to the multi pool.
      while ((running_handles < concurrency) &&
             (req_i < requests.size()) &&
             (!free_connections.empty()))
      {
         shared_ptr<CURL> conn = free_connections.top();
         const T &data = requests[req_i];
         shared_ptr<curl_oper> post = mk_request(data, conn, req_i);

         free_connections.pop();
         in_progress.push_back(post);

         CURLMcode status = curl_multi_add_handle(curl_multi.get(), post->m_curl.get());
         if (status != 0)
         {
            responses.at(req_i) = response_or_error_t("Error adding easy handle to curl_multi.");
         }
         
         req_i += 1;
      }

      int new_running_handles = running_handles;
      // perform CURL actions
      CURLMcode status;
      do {
         status = curl_multi_perform(curl_multi.get(), &new_running_handles); 
         // curl wants us to keep running this while it's returning this 
         // error code - presumably to drain all the incoming input.
      } while (status == CURLM_CALL_MULTI_PERFORM);

      // wait for something to happen - curl tells us how long it expects
      // to have to wait for something. note that this is *after* the curl
      // event gathering loop above so that curl has had an opportunity to
      // figure out exactly what it is that it is waiting for.
      long timeout = 0;
      curl_multi_timeout(curl_multi.get(), &timeout);
      if (timeout != 0) 
      {
         // first, pull the interesting file descriptors out of curl
         fd_set read_fd, write_fd, exc_fd;
         int n_fd = 0;
         FD_ZERO(&read_fd);
         FD_ZERO(&write_fd);
         FD_ZERO(&exc_fd);

         curl_multi_fdset(curl_multi.get(), &read_fd, &write_fd, &exc_fd, &n_fd);

         if (n_fd > 0)
         {
            // if there's timeout information then use that, otherwise
            // block until something interesting happens...
            if (timeout < 0)
            {
               timeout = 100;
            }
            else if (timeout > 1000)
            {
               timeout = 1000;
            }
             
            struct timeval tv;
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
            select(n_fd, &read_fd, &write_fd, &exc_fd, &tv);
         }
      }               

      // check if some actions completed
      struct CURLMsg *msg = NULL;
      int msg_count = 0;

      while ((msg = curl_multi_info_read(curl_multi.get(), &msg_count)) != NULL)
      {
         // curl docs say this should always be the case - that the other
         // values of CURLMSG enum are unused...
         if (msg->msg == CURLMSG_DONE)
         {
            CURLcode status = msg->data.result;
            
            // find connection & resp
            list<shared_ptr<curl_oper> >::iterator itr = in_progress.begin();
            while (itr != in_progress.end()) 
            {
               if (msg->easy_handle == (*itr)->m_curl.get())
               {
                  break;
               }
               ++itr;
            }
            
            if (itr != in_progress.end())
            {
               // take it out of the multi and the in_progress
               shared_ptr<curl_oper> oper = *itr;
               in_progress.erase(itr);
               
               // remove the connection from the multi
               curl_multi_remove_handle(curl_multi.get(), oper->m_curl.get());
               
               // assign response to output vector using index
               responses.at(oper->m_index) = oper->finish(status);
               
               // add connection back into free pool
               free_connections.push(oper->m_curl);

#ifdef HTTP_DEBUG
               LOG_FINER(boost::format("%1% <%2% %3%> <pending>")
                         % (oper->m_curl.get() != NULL ? "Persistent" : "")
                         % oper->oper_type() % oper->m_url);
#endif
            }
            else
            {
               // this is a serious logic error, so keep the exception
               // behaviour here.
               throw runtime_error("Request finished, but cannot find matching record.");
            }
         }
         else
         {
            // this is supposed not to happen, according to the cURL 
            // docs, so keep the exception behaviour here.
            throw runtime_error("Expected CURLMSG_DONE as status, but it was something else.");
         }
      }

      // update number of running handles for the next pass.
      running_handles = (size_t)new_running_handles;

      // break from the loop if there's nothing left to do
      if ((running_handles == 0) &&
          (req_i == requests.size()))
      {
         break;
      }
   }

   // filter the responses for errors here.
   vector<shared_ptr<response> > real_responses(responses.size());
   for (size_t i = 0; i < responses.size(); ++i)
   {
      real_responses[i] = boost::apply_visitor(response_or_error_visitor(), responses[i]);
   }
   
   return real_responses;
}

/* functor which makes a curl_post_single object from a pair
 * of the URL and the upload body.
 */
struct mk_request_single
{
   mk_request_single(const vector<string> &headers,
                     const bool &keepHeaders)
      : m_headers(headers), m_keep_headers(keepHeaders)
   {}

   shared_ptr<curl_oper> operator()(
      const pair<string, string> &req, 
      shared_ptr<CURL> conn, 
      size_t idx) const 
   {
      part data(req.second.c_str(), req.second.size());
      shared_ptr<curl_oper> post(new curl_post_single(conn, req.first, data, m_headers, m_keep_headers, idx));
      return post;
   }

   const vector<string> &m_headers;
   const bool m_keep_headers;
};

/* functor which makes a curl_post_form object from an input
 * pair of the URL and the array of multi-part form objects.
 */
struct mk_request_form
{
   mk_request_form(const vector<string> &headers,
                   const bool &keepHeaders,
                   const bool &submit)
      : m_headers(headers), m_keep_headers(keepHeaders),
        m_submit(submit)
   {}

   shared_ptr<curl_oper> operator()(
      const pair<string, vector<part> > &req, 
      shared_ptr<CURL> conn, 
      size_t idx) const 
   {
      shared_ptr<curl_oper> post(new curl_post_form(conn, req.first, req.second, m_headers, m_keep_headers, m_submit, idx));
      return post;
   }

   const vector<string> &m_headers;
   const bool m_keep_headers;
   const bool m_submit;
};

/* functor to create GET request operation objects.
 */
struct mk_request_get
{
   mk_request_get(const vector<string> &headers,
                  const bool &keepHeaders)
      : m_headers(headers), m_keep_headers(keepHeaders)
   {}

   shared_ptr<curl_oper> operator()(
      const string &req, 
      shared_ptr<CURL> conn, 
      size_t idx) const 
   {
      return shared_ptr<curl_oper>(new curl_get(conn, req, m_headers, m_keep_headers, idx, 0L));
   }

   const vector<string> &m_headers;
   const bool m_keep_headers;
};
   
} // anonymous namespace

namespace http
{
   shared_ptr<CURL> createPersistentConnection()
   {
      //TODO: make sure that curl_easy_cleanup really gets called on the CURL* created here
      //return a handle, telling the shared pointer to delete using the curl cleanup function
      return shared_ptr<CURL>(curl_easy_init(), &curl_easy_cleanup);
   }

shared_ptr<response> get(
   const string &url, 
   shared_ptr<CURL> connection, 
   const vector<string>& headers, 
   const bool& keepHeaders, 
   long timeout)
{
      curl_get cg(connection, url, headers, keepHeaders, timeout);

      //do the request
      CURLcode status = curl_easy_perform(cg.m_curl.get());

      response_or_error_t maybe_resp = cg.finish(status);
      shared_ptr<response> resp = boost::apply_visitor(response_or_error_visitor(), maybe_resp);

#ifdef HTTP_DEBUG
      LOG_FINER(boost::format("%1% <GET %2%> <STATUS %3%>") 
                % (connection.get() != NULL ? "Persistent" : "")
                % cg.m_url % resp->statusCode);
#endif
      return resp;
   }

   // perform HTTP post, returning the response
   shared_ptr<response> post(const string &url, const string &post, shared_ptr<CURL> connection, const vector<string>& headers, const bool& keepHeaders)
   {
      //setup the request
      part data(post.c_str(), post.length());
      curl_post_single cp(connection, url, data, headers, keepHeaders);

      //do the request
      CURLcode status = curl_easy_perform(cp.m_curl.get());

      response_or_error_t maybe_resp = cp.finish(status);
      shared_ptr<response> resp = boost::apply_visitor(response_or_error_visitor(), maybe_resp);

#ifdef HTTP_DEBUG
      LOG_FINER(boost::format("%1% <POST %2%> <STATUS %3%>")
                % (connection.get() != NULL ? "Persistent" : "")
                % cp.m_url % resp->statusCode);
#endif
      return resp;
   }

   // perform HTTP post of multipart form, returning the response
   shared_ptr<response> postForm(const string &url, const vector<part>& parts, shared_ptr<CURL> connection, const vector<string>& headers, const bool& keepHeaders, const bool& submit)
   {
      curl_post_form cp(connection, url, parts, headers, keepHeaders, submit);

      //do the request
      CURLcode status = curl_easy_perform(cp.m_curl.get());

      response_or_error_t maybe_resp = cp.finish(status);
      shared_ptr<response> resp = boost::apply_visitor(response_or_error_visitor(), maybe_resp);

#ifdef HTTP_DEBUG
      LOG_FINER(boost::format("%1% <POST %2%> <STATUS %3%>")
                % (connection.get() != NULL ? "Persistent" : "")
                % url.c_str() % resp->statusCode);
#endif
      return resp;
   }

// perform HTTP multi-get, returning responses in the same order as requests.
vector<shared_ptr<response> > multiGet(
   const vector<string> &urls,
   size_t concurrency,
   shared_ptr<CURL> connection,
   const vector<string> &headers,
   const bool &keepHeaders)
{
   mk_request_get mk_req(headers, keepHeaders);

   return do_multi_requests<string>(urls, mk_req, concurrency, connection);
}

// perform HTTP multi-post, returning the responses in the same order as the requests.
// each request is a pair<string, string> of the URL and the data to post.
vector<shared_ptr<response> > multiPost(
   const vector<pair<string, string> > &requests,
   size_t concurrency,
   shared_ptr<CURL> connection,
   const vector<string> &headers,
   const bool &keepHeaders)
{
   mk_request_single mk_req(headers, keepHeaders);

   return do_multi_requests<pair<string, string> >(requests, mk_req, concurrency, connection);
}

// perform HTTP multi-post with forms, returning the responses in the same order 
// as the requests. each request is a pair<string, vector<part>> of the URL and 
// the form parts to post.
vector<shared_ptr<response> > multiPostForm(
   const vector<pair<string, vector<part> > > &requests,
   size_t concurrency,
   shared_ptr<CURL> connection,
   const vector<string> &headers,
   const bool &keepHeaders,
   const bool &submit)
{
   mk_request_form mk_req(headers, keepHeaders, submit);

   return do_multi_requests<pair<string, vector<part> > >(requests, mk_req, concurrency, connection);
}

   // perform HTTP delete, returning the response
   shared_ptr<response> del(const string &url, shared_ptr<CURL> connection, const vector<string>& headers, const bool& keepHeaders)
   {
      //try to reuse the previous connection
      CURL *curl = connection.get();
      //if we didnt have a persistent
      if(curl == NULL)
         curl = curl_easy_init();
      //we need to reset the options used in the last call
      else
         curl_easy_reset(curl);

      if(curl != 0)
      {
         //setup the request
         shared_ptr<response> resp(new response());
         curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
         curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
         curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
         curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp.get());

         //send headers along
         struct curl_slist* headerList = NULL;
         for(vector<string>::const_iterator header = headers.begin(); header != headers.end(); header++)
         {
            headerList = curl_slist_append(headerList, header->c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
         }

         //keep the headers or not
         if(keepHeaders)
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
         else
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, discard_callback);
         curl_easy_setopt(curl, CURLOPT_HEADERDATA, resp.get());

         //do the request
         CURLcode status = curl_easy_perform(curl);
         if(status != 0)
         {
            ostringstream ostr;
            ostr << "CURL error on DELETE to `" << url << "',:" << curl_easy_strerror(status);
            throw runtime_error(ostr.str());
         }
         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->statusCode);
         if(headerList)
            curl_slist_free_all(headerList);
         //this wasn't a persistent connection so get rid of it
         if(connection.get() == NULL)
            curl_easy_cleanup(curl);

#ifdef HTTP_DEBUG
         LOG_FINER(boost::format("%1% <DELETE %2%> <STATUS %3%>")
                   % (connection.get() != NULL ? "Persistent" : "")
                   % url.c_str() % resp->statusCode);
#endif
         return resp;
      }
      else
      {
         throw runtime_error("Cannot initialise CURL library.");
      }
   }

   // perform HTTP head, returning the response
   shared_ptr<response> head(const string &url, shared_ptr<CURL> connection, const vector<string>& headers, const bool& keepHeaders)
   {
      //try to reuse the previous connection
      CURL *curl = connection.get();
      //if we didnt have a persistent
      if(curl == NULL)
         curl = curl_easy_init();
      //we need to reset the options used in the last call
      else
         curl_easy_reset(curl);

      if(curl != 0)
      {
         //setup the request
         shared_ptr<response> resp(new response());
         curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
         curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
         curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
         curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
         curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp.get());
         curl_easy_setopt(curl, CURLOPT_FILETIME, 1L);

         //send headers along
         struct curl_slist* headerList = NULL;
         for(vector<string>::const_iterator header = headers.begin(); header != headers.end(); header++)
         {
            headerList = curl_slist_append(headerList, header->c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
         }

         //keep the headers or not
         if(keepHeaders)
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
         else
            curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, discard_callback);
         curl_easy_setopt(curl, CURLOPT_HEADERDATA, resp.get());

         //do the request
         CURLcode status = curl_easy_perform(curl);
         if(status != 0)
         {
            ostringstream ostr;
            ostr << "CURL error on HEAD to `" << url << "',:" << curl_easy_strerror(status);
            throw runtime_error(ostr.str());
         }
         curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &resp->statusCode);
         curl_easy_getinfo(curl, CURLINFO_FILETIME, &resp->timeStamp);
         if(headerList)
            curl_slist_free_all(headerList);
         //this wasn't a persistent connection so get rid of it
         if(connection.get() == NULL)
            curl_easy_cleanup(curl);

#ifdef HTTP_DEBUG
         LOG_FINER(boost::format("%1% <HEAD %2%> <STATUS %3%>")
                   % (connection.get() != NULL ? "Persistent" : "")
                   % url.c_str() % resp->statusCode);
#endif
         return resp;
      }
      else
      {
         throw runtime_error("Cannot initialise CURL library.");
      }
   }

   std::string escape_url(const std::string &url, shared_ptr<CURL> connection)
   {
      //try to reuse the previous connection
      CURL *curl = connection.get();
      //if we didnt have a persistent
      if(curl == NULL)
         curl = curl_easy_init();
      //we need to reset the options used in the last call
      else
         curl_easy_reset(curl);

      if (curl == 0)
      {
         throw std::runtime_error("Cannot initialise CURL library.");
      }

      char *escaped_c_str = curl_easy_escape(curl, url.c_str(), url.size());
      string escaped(escaped_c_str);
      curl_free(escaped_c_str);
      //this wasn't a persistent connection so get rid of it
      if(connection.get() == NULL)
         curl_easy_cleanup(curl);

      return escaped;
   }

   std::string unescape_url(const std::string &url, shared_ptr<CURL> connection)
   {
      //try to reuse the previous connection
      CURL *curl = connection.get();
      //if we didnt have a persistent
      if(curl == NULL)
         curl = curl_easy_init();
      //we need to reset the options used in the last call
      else
         curl_easy_reset(curl);

      if (curl == 0)
      {
         throw std::runtime_error("Cannot initialise CURL library.");
      }

      int outlength = 0;
      char *unescaped_c_str = curl_easy_unescape(curl, url.c_str(), url.size(), &outlength);
      string unescaped(unescaped_c_str, outlength);
      curl_free(unescaped_c_str);
      //this wasn't a persistent connection so get rid of it
      if(connection.get() == NULL)
         curl_easy_cleanup(curl);

      return unescaped;
   }

} // namespace http
