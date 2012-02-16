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

#ifndef ZSTREAM_PBUF_HPP
#define ZSTREAM_PBUF_HPP

#include "zstream.hpp"
#include "tile_protocol.hpp"

namespace rendermq {

// output stream helper, knows how to serialise a tile_protocol using 
// google protocol buffers.
zstream::socket::osocket &operator<<(zstream::socket::osocket &out,
                                     const tile_protocol &tile);

// input stream helper, knows how to deserialise a tile_protocol using 
// google protocol buffers.
zstream::socket::isocket &operator>>(zstream::socket::isocket &in,
                                     tile_protocol &tile);

}

#endif // ZSTREAM_PBUF_HPP
