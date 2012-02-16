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

#include <string>
#include <vector>
#include <stdexcept>

#include "tile_utils.hpp"

namespace rendermq {

const std::string mime_json("application/json;charset=UTF-8");
const std::string mime_png("image/png");
const std::string mime_jpg("image/jpeg");
const std::string mime_gif("image/gif");

const std::string file_none("none");
const std::string file_json("json");
const std::string file_png("png");
const std::string file_jpg("jpeg");
const std::string file_gif("gif");
const std::string file_all("all");

const std::string &mime_type_for(const rendermq::protoFmt& fmt) {
   switch (fmt) {
   case rendermq::fmtPNG:  return mime_png;
   case rendermq::fmtJPEG: return mime_jpg;
   case rendermq::fmtGIF: return mime_gif;
   case rendermq::fmtJSON: return mime_json;
   default:
      throw std::runtime_error("Ambiguous format in mime_type_for()");
   }
}

const std::string &file_type_for(const rendermq::protoFmt& fmt) {
   switch (fmt) {
   case rendermq::fmtNone: return file_none;
   case rendermq::fmtPNG:  return file_png;
   case rendermq::fmtJPEG: return file_jpg;
   case rendermq::fmtGIF:  return file_gif;
   case rendermq::fmtJSON: return file_json;
   case rendermq::fmtAll:  return file_all;
   default:
      throw std::runtime_error("Ambiguous format in file_type_for()");
   }
}

const std::vector<protoFmt> get_formats_vec(const rendermq::protoFmt& formatMask)
{
   //peel each format out of the mask and add to a vector
   std::vector<protoFmt> formats;
   if(formatMask & rendermq::fmtPNG) formats.push_back(rendermq::fmtPNG);
   if(formatMask & rendermq::fmtJPEG) formats.push_back(rendermq::fmtJPEG);
   if(formatMask & rendermq::fmtGIF) formats.push_back(rendermq::fmtGIF);
   if(formatMask & rendermq::fmtJSON) formats.push_back(rendermq::fmtJSON);
   return formats;
}

const protoFmt get_format_for(const std::string& fileType)
{
   if(fileType == file_json)
      return rendermq::fmtJSON;
   else if(fileType == file_png)
      return rendermq::fmtPNG;
   else if(fileType == file_jpg)
      return rendermq::fmtJPEG;
   else if(fileType == file_gif)
      return rendermq::fmtGIF;
   else
      return rendermq::fmtNone;
}

} // rendermq namespace

