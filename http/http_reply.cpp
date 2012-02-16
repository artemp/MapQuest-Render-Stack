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

#include "http_reply.hpp"
#include <sstream>

#define SERVER_VERSION "0.8.0"
#define SERVER_NAME "Mapnik2"
#define SERVER SERVER_NAME "/" SERVER_VERSION
#define SUPPORT_ADDRESS "http://developer.mapquest.com/web/info/contact"

namespace rendermq
{

void send_reply(zmq::socket_t & socket, 
                std::string const& uuid, 
                std::string const& id , 
                int status, 
                std::string const& output)
{
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << status << " " << "OK" << "\r\n";
   http << "Content-Type: text/plain\r\n";
   http << "Content-Length: " << output.length()  << "\r\n";
   http << "Server: " SERVER "\r\n\r\n";
   http << output;
   std::string s = http.str();
   zmq::message_t msg(s.length());
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);
}

void send_404(zmq::socket_t & socket,
              std::string const& uuid, 
              std::string const& id)
{
   std::string output("Sorry - we haven't been able to serve the content you asked for\n");
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 404 << " " << "Not Found" << "\r\n";
   http << "Content-Type: text/plain\r\n";
   http << "Content-Length: " << output.length()  << "\r\n";
   http << "Server: " SERVER "\r\n\r\n";
   http << output;
   std::string s = http.str();
   zmq::message_t msg(s.length());
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);
}

void send_304(zmq::socket_t & socket, 
              std::string const& uuid, 
              std::string const& id,
              std::time_t date, 
              http_date_formatter const& formatter,
              const std::string &mime_type)
{
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 304 << " " << "Not Modified" << "\r\n";
   http << "Content-Type: " << mime_type << "\r\n";
   http << "Date: ";
   formatter(http,date);
   http << "\r\n";
   http << "Server: " SERVER "\r\n\r\n";
   std::string s = http.str();
   zmq::message_t msg(s.length());
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);
}

void send_tile(zmq::socket_t & socket, http_date_formatter const& frmt,
               std::string const& uuid, std::string const& id ,
               unsigned max_age , std::time_t last_modified, std::time_t expire_time,
               std::string const& data, const std::string &mime_type)
{
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 200 << " " << "OK" << "\r\n";
   http << "Content-Type: " << mime_type << "\r\n";
   http << "Content-Length: " << data.length()  << "\r\n";
   http << "Cache-Control: max-age=" << max_age << "\r\n";
   http << "Edge-Control: downstream-ttl=" << max_age << "\r\n";
   http << "Last-Modified: ";
   frmt(http,last_modified);
   http << "\r\n";
   http << "Expires: " ;
   frmt(http,expire_time);
   http << "\r\n";
   http << "Server: " SERVER "\r\n";
   http << "Access-Control-Allow-Origin: *\r\n\r\n";
   http << data;
   std::string s = http.str();
   zmq::message_t msg(s.length()); 
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);    
}

void send_tile(zmq::socket_t & socket, 
               std::string const& uuid, 
               std::string const& id ,
               std::string const& data,
               const std::string &mime_type)
{
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 200 << " " << "OK" << "\r\n";
   http << "Content-Type: " << mime_type << "\r\n";
   http << "Content-Length: " << data.length()  << "\r\n";
   http << "Pragma: no-cache\r\n";
   http << "Cache-Control: no-cache\r\n";
   http << "Server: " SERVER "\r\n";
   http << "Access-Control-Allow-Origin: *\r\n\r\n";
   http << data;
   std::string s = http.str();
   zmq::message_t msg(s.length()); 
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);    
}

void send_500(zmq::socket_t &socket,
              const std::string &uuid,
              const std::string &id) {
   std::string output("An unexpected error has occurred. This is a bug, please report it at " SUPPORT_ADDRESS "\n");
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 500 << " " << "Server Error" << "\r\n";
   http << "Content-Type: text/plain\r\n";
   http << "Content-Length: " << output.length()  << "\r\n";
   http << "Server: " SERVER "\r\n\r\n";
   http << output;
   std::string s = http.str();
   zmq::message_t msg(s.length());
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);
}

void send_503(zmq::socket_t &socket,
              const std::string &uuid,
              const std::string &id) {
   std::string output("The service is overloaded and cannot presently complete your request. Please try again later.\n");
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 503 << " " << "Service Overloaded" << "\r\n";
   http << "Content-Type: text/plain\r\n";
   http << "Content-Length: " << output.length()  << "\r\n";
   http << "Server: " SERVER "\r\n\r\n";
   http << output;
   std::string s = http.str();
   zmq::message_t msg(s.length());
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);
}

void send_202(zmq::socket_t &socket,
              const std::string &uuid,
              const std::string &id) {
   std::string output("The tile you requested is not available at the moment. Please try again later.\n");
   std::ostringstream http;
   http << uuid << " " << id.size() << ":" << id << ", ";
   http << "HTTP/1.1" << " " << 202 << " " << "Accepted" << "\r\n";
   http << "Content-Type: text/plain\r\n";
   http << "Content-Length: " << output.length()  << "\r\n";
   http << "Cache-Control: no-cache\r\n";
   http << "Pragma: no-cache\r\n";
   http << "Server: " SERVER "\r\n\r\n";
   http << output;
   std::string s = http.str();
   zmq::message_t msg(s.length());
   std::memcpy(msg.data(),s.c_str(),s.length());
   socket.send(msg);
}


}
