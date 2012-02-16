#!/bin/bash

if [ -z $1 ]; then
	echo "Usage $0 packageName"
	exit
fi

#pretty boring stuff here
date=`date +%s`
mkdir -p $date
rsync -aqvz --exclude ".svn" --exclude "$date" --exclude "`basename $0`" * ./$date
cd ./$date
tar pczf ../$1 *
cd ..
rm -rf ./$date
md5sum $1
