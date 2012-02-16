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

#ifndef ZSTREAM_HPP
#define ZSTREAM_HPP

#include <zmq.hpp>
#include <string>
#include <list>
#include <boost/noncopyable.hpp>
#include <stdint.h>

/* wrap 0MQ to provide a stream-like interface. 
 *
 * i've observed that, without this it seems there's a lot of boilerplate
 * with sending and receiving stuff and it seems that there's a lot of scope
 * for making the code much more readable and less error-prone if the rather
 * minimalist 0MQ C++ interface were extended a little.
 *
 * this also allows the injection of semantics into the type system, e.g:
 * that PUB-type sockets cannot receive messages and SUB-type ones cannot
 * send them.
 */
namespace zstream {

namespace manip {

/* output manipulation for sending multi-part messages
 *
 * for example, to send a multi-part message consisting of the two strings
 * "Hello" and "World":
 *   namespace manip = zstream::manip;
 *   zstream::socket::pub broadcast(ctx);
 *   broadcast << manip::more << "Hello" << "World";
 */
struct manip_more {};
extern manip_more more;

// to pull out and discard multi-part headers
struct manip_ignore_routing_headers {};
extern manip_ignore_routing_headers ignore_routing_headers;

// to pull out and preserve multi-part headers
struct routing_headers {
  routing_headers(std::list<std::string> &h) : headers(h) {}
  std::list<std::string> &headers;
};

} // namespace manip

namespace socket {

/* base class for all sockets. concrete, but not directly constructable. 
 * use one of the derived classes to construct a working socket with a
 * type-system-enforced policy.
 */
class basic_socket : public boost::noncopyable {
protected:
  zmq::socket_t s_;
  basic_socket(zmq::context_t &, int);

public:  
  // closes the socket
  ~basic_socket();

  // same as zmq::socket_t::bind
  void bind(const std::string &);

  // same as zmq::socket_t::connect
  void connect(const std::string &);

  // set the identity of this socket. argument must be between
  // one and 255 characters in length and can be non-alphanumeric,
  // but i wouldn't recommend it.
  void set_identity(const std::string &);

  // gets the identity of the socket. note that if no identity has
  // been set, 0MQ assigns one. *however* that automatically-assigned
  // identity isn't available - you'll just get a blank string.
  std::string identity() const;

  // the void pointer suitable for putting into a pollitem_t struct
  // so you can use it with other socket types in zmq::poll.
  void *socket();

   zmq::socket_t &nasty_hack() { return s_; }
};

/* an output-only socket.
 */
class osocket : public virtual basic_socket {
public:
  // send a message structure. note that it's a non-const reference
  // due to the requirements of the 0MQ C++ API.
  osocket &operator<<(zmq::message_t &);

  // pack a string into a message and send it over the socket.
  osocket &operator<<(const std::string &);

  // operators for receiving common types of integers
  osocket &operator<<(uint32_t);
  osocket &operator<<(uint64_t);
  
  // set a stream manipulation bit
  osocket &operator<<(const manip::manip_more &);

protected:
  osocket(zmq::context_t &, int);

private:
  // whether there are more messages to come after the next.
  bool more_;
};

/* an input-only socket.
 */
class isocket : public virtual basic_socket {
public:
  // retrieve a message from the socket
  isocket &operator>>(zmq::message_t &);

  // retrieve a message from the socket and store its data in
  // a string object.
  isocket &operator>>(std::string &);

  // operators for sending common types of integers
  isocket &operator>>(uint32_t &);
  isocket &operator>>(uint64_t &);
  
  // true if the last message received is not the last in a 
  // multi-part message.
  bool has_more() const;

protected:
  isocket(zmq::context_t &, int);

private:
  // whether there's more messages in a multi-part message still
  // to come.
  bool more_;
};

/* a routed input socket - this encapsulates the 0MQ behaviour
 * when reading from a socket which prepends the routing information.
 */
class ixsocket : public isocket {
public:
  using isocket::operator>>;

  // read and discard the routing headers
  ixsocket &operator>>(const manip::manip_ignore_routing_headers &);

  // read routing headers into a list<string>
  ixsocket &operator>>(manip::routing_headers &);

protected:
  ixsocket(zmq::context_t &, int);
};

/* A publish socket.
 */
class pub : public osocket {
public:
  pub(zmq::context_t &);
};

/* A subscription socket.
 */
class sub : public isocket {
public:
  sub(zmq::context_t &);

  // TODO: add explicit subscription management function
};

/* a strict request-reply socket.
 */
class req : public osocket, public isocket {
public:
  req(zmq::context_t &);
};

/* a strict responder (reply-request) socket.
 */
class rep : public osocket, public isocket {
public:
  rep(zmq::context_t &);
};

/* a routable request socket. this load balances over the other
 * endpoints connected to it.
 */
class xreq : public ixsocket, public osocket {
public:
  xreq(zmq::context_t &);
};

/* XREP sockets are routed when sending and this is wrapped by
 * not exposing the osocket functionality of this class directly,
 * but needing to call the to() method, which sets up the 
 * addressing information and returns an osocket.
 */
class xrep : public ixsocket, protected osocket {
public:
  xrep(zmq::context_t &);

  // get an osocket to the addressee
  osocket &to(const std::string &);

  // get an osocket to a list of addressees, used for multi-hop
  // routing.
  osocket &to(const std::list<std::string> &);
};

/* a point-to-point matched pair socket. this type of socket is
 * recommended by the 0MQ docs for inproc:// communication between
 * threads.
 */
class pair : public osocket, public isocket {
public:
  pair(zmq::context_t &);
};

/* push/pull sockets are unidirectional fan-out and reduction 
 * sockets. this is good for distributing work evenly amongst
 * many workers (in a round-robin fashion).
 */
class push : public osocket {
public:
  push(zmq::context_t &);
};
class pull : public isocket {
public:
  pull(zmq::context_t &);
};

} // namespace zstream::socket

} // namespace zstream

#endif // ZSTREAM_HPP
