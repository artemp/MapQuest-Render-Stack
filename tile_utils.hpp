/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: john.novak@mapquest.com
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

#ifndef RENDERMQ_TILE_UTILS_HPP
#define RENDERMQ_TILE_UTILS_HPP

#include <string>
#include <vector>

namespace rendermq {

/* intended to be used as a bitmask - a tile request could be for 
 * several formats at once. note that not all commands depend on
 * the format(s). */
enum protoFmt {
  fmtNone = 0,
  fmtPNG  = 1, 
  fmtJPEG = 2,
  fmtJSON = 4, 
  fmtGIF  = 8,
  fmtAll = 15
  // NOTE: because it's used as a bit-mask, all enum values need
  // to be powers of two.
};

const std::string &mime_type_for(const rendermq::protoFmt& fmt);
const std::string &file_type_for(const rendermq::protoFmt& fmt);
const std::vector<protoFmt> get_formats_vec(const rendermq::protoFmt& formatMask);
const protoFmt get_format_for(const std::string& fileType);

} // rendermq namespace

#endif // RENDERMQ_TILE_UTILS_HPP
