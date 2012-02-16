/*------------------------------------------------------------------------------
 *
 * Null handle - a handle which doesn't exist, hasn't been 
 * modified, has no data, etc...
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2011 Mapquest, Inc.  All Rights reserved.
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

#include "null_handle.hpp"

namespace rendermq {

using std::time_t;
using std::string;

null_handle::null_handle()
{
}

null_handle::~null_handle()
{
}

bool null_handle::exists() const
{
   return false;
}

time_t null_handle::last_modified() const
{
   return time_t(0);
}

bool null_handle::data(string &) const 
{
   return false;
}

bool null_handle::expired() const 
{
   return false;
}

}
