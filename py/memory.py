#------------------------------------------------------------------------------
#
#  Utility method to get memory usage.
#
#  Author: matt.amos@mapquest.com
#
#  Copyright 2010-2 Mapquest, Inc.  All Rights reserved.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#
#------------------------------------------------------------------------------

import os
import resource

def get_virtual_size():
    """Returns the total virtual size of this process in bytes.
    Note that this only works on Linux, as it uses /proc.
    """
    status_file = '/proc/%d/statm' % os.getpid()
    try:
        with open(status_file) as f:
            size, resident, share, text, lib, data, dt = map(lambda x: int(x), f.read().split(" "))
        return resource.getpagesize() * size
    except:
        return -1

    
