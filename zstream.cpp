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

#include "zstream.hpp"
#include <cstring> // for std::memcpy
#include <stdint.h> // for uint64_t
#include <iostream>
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>

// for endianness conversion routines
//#define _BSD_SOURCE
#include <endian.h>
#include <byteswap.h>

#ifndef htobe16

# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define htobe16(x) __bswap_16 (x)
#  define htole16(x) (x)
#  define be16toh(x) __bswap_16 (x)
#  define le16toh(x) (x)

#  define htobe32(x) __bswap_32 (x)
#  define htole32(x) (x)
#  define be32toh(x) __bswap_32 (x)
#  define le32toh(x) (x)

#  define htobe64(x) __bswap_64 (x)
#  define htole64(x) (x)
#  define be64toh(x) __bswap_64 (x)
#  define le64toh(x) (x)
# else
#  define htobe16(x) (x)
#  define htole16(x) __bswap_16 (x)
#  define be16toh(x) (x)
#  define le16toh(x) __bswap_16 (x)

#  define htobe32(x) (x)
#  define htole32(x) __bswap_32 (x)
#  define be32toh(x) (x)
#  define le32toh(x) __bswap_32 (x)

#  define htobe64(x) (x)
#  define htole64(x) __bswap_64 (x)
#  define be64toh(x) (x)
#  define le64toh(x) __bswap_64 (x)
# endif
#endif

using std::string;
using std::list;
using std::ostringstream;

namespace {
string make_safe(const void *ptr, size_t length) {
  std::ostringstream ostr;
  const char *str = (const char *)ptr;
  for (size_t i = 0; i < length; ++i) {
    unsigned char c = str[i];
    if ((c >= 0x20) && (c <= 0x7e)) {
      ostr << str[i];
    } else {
      ostr << ".";
    }
  }
  return ostr.str();
} 

// utility function for sending POD types over 0MQ. note that
// POD types must be in a common byte order for portability.
template <typename T>
zstream::socket::osocket &pod_send(zstream::socket::osocket &out, T t) {
  zmq::message_t msg(sizeof(T));
  T *ptr = (T *)msg.data();
  *ptr = t;
  return out << msg;
}

// utility function for receiving POD types over 0MQ. returned
// types will be in common (network) byte order.
template <typename T>
void pod_recv(zstream::socket::isocket &in, T &t) {
  zmq::message_t msg;
  in >> msg;
  if (msg.size() != sizeof(T)) {
    ostringstream ostr;
    ostr << "Recieved message of incorrect size for POD type. Was expecting "
         << sizeof(T) << " bytes, but received " << msg.size() << " ("
         << make_safe(msg.data(), msg.size()) << ").";
    throw std::runtime_error(ostr.str());
  }
  T *ptr = (T *)msg.data();
  t = *ptr;
}
} // anonymous namespace

namespace zstream {

namespace manip {

// concrete instance of more
manip_more more;

// concrete instance of routing_headers
manip_ignore_routing_headers ignore_routing_headers;

} // namespace manip

namespace socket {

basic_socket::basic_socket(zmq::context_t &ctx, int type)
   : s_(ctx, type) {
}

basic_socket::~basic_socket()
{
   // close happens automatically with zmq c++ wrapper
}
  
void 
basic_socket::bind(const std::string &addr) {
  try {
    s_.bind(addr.c_str());
  } catch (const zmq::error_t &e) {
    std::cerr << "ERROR: unable to bind `" << addr << "': " << e.what() << std::endl;
    throw;
  }
}

void 
basic_socket::connect(const std::string &addr) {
  s_.connect(addr.c_str());
}

void 
basic_socket::set_identity(const std::string &id) {
  s_.setsockopt(ZMQ_IDENTITY, id.data(), id.length());
}

std::string 
basic_socket::identity() const {
  char buf[255];
  size_t buf_len = sizeof buf;

  // yeah, the const-cast is nasty, but we're inheriting this from the
  // 0MQ C++ bindings, so i'm putting the blame firmly there. ;-)
  const_cast<zmq::socket_t &>(s_).getsockopt(ZMQ_IDENTITY, buf, &buf_len);

  return string(buf, buf_len);
}

void *
basic_socket::socket() {
  return s_;
}

osocket::osocket(zmq::context_t &ctx, int type) 
  : basic_socket(ctx, type), more_(false) {
}

osocket &
osocket::operator<<(zmq::message_t &msg) {
  // since we're not setting non-blocking mode the only return value is
  // true - or it throws an error.
  s_.send(msg, more_ ? ZMQ_SNDMORE : 0);
  more_ = false;
  return *this;
}

osocket &
osocket::operator<<(const std::string &str) {
  zmq::message_t msg(str.size());
  std::memcpy(msg.data(),str.data(),str.size());
  return operator<<(msg);
}

osocket &
osocket::operator<<(uint32_t x) {
  return pod_send(*this, htobe32(x));
}

osocket &
osocket::operator<<(uint64_t x) {
  return pod_send(*this, htobe64(x));
}

osocket &
osocket::operator<<(const manip::manip_more &) {
  more_ = true;
  return *this;
}

isocket::isocket(zmq::context_t &ctx, int type) 
  : basic_socket(ctx, type), more_(false) {
}

isocket &
isocket::operator>>(zmq::message_t &msg) {
  int64_t more;
  size_t more_size = sizeof (more);
  s_.recv(&msg);
  s_.getsockopt(ZMQ_RCVMORE, &more, &more_size);
  more_ = more != 0;
  return *this;
}

isocket &
isocket::operator>>(std::string &str) {
  zmq::message_t msg;
  operator>>(msg);
  str.assign((const char *)msg.data(), msg.size());
  return *this;
}

isocket &
isocket::operator>>(uint32_t &x) {
  uint32_t nx;
  pod_recv(*this, nx);
  x = be32toh(nx);
  return *this;
}
  
isocket &
isocket::operator>>(uint64_t &x) {
  uint64_t nx;
  pod_recv(*this, nx);
  x = be64toh(nx);
  return *this;
}
  
bool 
isocket::has_more() const {
  return more_;
}

ixsocket::ixsocket(zmq::context_t &ctx, int type)
  : basic_socket(ctx, type), 
    isocket(ctx, type) {
}

ixsocket &
ixsocket::operator>>(const manip::manip_ignore_routing_headers &) {
  zmq::message_t msg;

  // routing headers are a multi-part message delimited by
  // a blank message. if we don't get the blank message then
  // just consume the whole message.
  do {
    isocket::operator>>(msg);
  } while (has_more() && (msg.size() > 0));

  return *this;
}

ixsocket &
ixsocket::operator>>(manip::routing_headers &r) {
  std::string str;

  // make sure there's nothing left over from any previous
  // set of routing headers.
  r.headers.clear();

  do {
    isocket::operator>>(str);
    
    // a zero-size message indicates end of headers
    if (str.length() == 0) {
      break;
    } else 
      r.headers.push_back(str);

  } while (has_more());

  return *this;
}

pub::pub(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_PUB),
    osocket(ctx, ZMQ_PUB) {
}

sub::sub(zmq::context_t &ctx) 
  : basic_socket(ctx, ZMQ_SUB),
    isocket(ctx, ZMQ_SUB) {
  string subscription;
  s_.setsockopt(ZMQ_SUBSCRIBE, subscription.data(), subscription.size());
}

req::req(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_REQ),
    osocket(ctx, ZMQ_REQ),
    isocket(ctx, ZMQ_REQ) {
}

rep::rep(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_REP),
    osocket(ctx, ZMQ_REP),
    isocket(ctx, ZMQ_REP) {
}

xreq::xreq(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_XREQ),
    ixsocket(ctx, ZMQ_XREQ),
    osocket(ctx, ZMQ_XREQ) {
}

xrep::xrep(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_XREP),
    ixsocket(ctx, ZMQ_XREP),
    osocket(ctx, ZMQ_XREP) {
}

// get an osocket to the addressee
osocket &
xrep::to(const std::string &addr) {
  // treat self as an osocket to send the routing headers
  // and return that for use.
  osocket &out = static_cast<osocket &>(*this);
  out << manip::more << addr
      << manip::more << "";
  return out;
}

osocket &
xrep::to(const list<string> &addrs) {
  osocket &out = static_cast<osocket &>(*this);
  for (list<string>::const_iterator addr = addrs.begin();
       addr != addrs.end(); ++addr) {
    out << manip::more << *addr;
  }
  out << manip::more << "";
  return out;
}

pair::pair(zmq::context_t &ctx) 
  : basic_socket(ctx, ZMQ_PAIR),
    osocket(ctx, ZMQ_PAIR),
    isocket(ctx, ZMQ_PAIR) {
}

push::push(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_PUSH),
    osocket(ctx, ZMQ_PUSH) {
}

pull::pull(zmq::context_t &ctx)
  : basic_socket(ctx, ZMQ_PULL),
    isocket(ctx, ZMQ_PULL) {
}

} // namespace zstream::socket

} // namespace zstream
