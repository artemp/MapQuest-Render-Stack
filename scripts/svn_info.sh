#!/bin/bash

root=$1;
local_mods=`svn stat -q $root | wc -l`;
if [ $local_mods -eq 0 ]; then
    mods_str="";
else
    mods_str=" (locally modified)";
fi
rev=`svn info $root | grep Revision: | awk '{print $2;}'`;

cat <<EOF
/* NOTE! This file is automatically generated by $0 - please do
 * not modify by hand!
 */
#ifndef VERSION_HPP
#define VERSION_HPP

#define VERSION "${rev}${mods_str}"

#endif /* VERSION_HPP */
EOF
