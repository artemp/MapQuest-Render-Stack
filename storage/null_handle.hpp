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

#ifndef RENDERMQ_NULL_HANDLE_HPP
#define RENDERMQ_NULL_HANDLE_HPP

#include <string>
#include <ctime>
#include "tile_storage.hpp"

namespace rendermq {

/* Null handle doesn't exist. This is a useful class to have
 * around so that the storage classes can reference it and
 * return a null handle when they hit an error, or don't have
 * the 
 */
class null_handle : public tile_storage::handle 
{
public:
   null_handle();
   ~null_handle();

   virtual bool exists() const;
   virtual std::time_t last_modified() const;
   virtual bool data(std::string &) const;
   virtual bool expired() const;
};

}

#endif // RENDERMQ_NULL_HANDLE_HPP
