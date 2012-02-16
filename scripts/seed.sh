#!/bin/bash
STDBASE=/data/mapquest/stdbase
BASE_DIR=/data/mapquest/render_stack
LOG_DIR=/data/mapquest/logs/render_stack

export PATH=$BASE_DIR/bin:$STDBASE/bin:/usr/bin:/bin
export LD_LIBRARY_PATH=$BASE_DIR/lib:$STDBASE/lib:$STDBASE/lib64

if [ -z "$1" ]; then
	echo "Usage: ${0} file1 file2 ..."
	exit
fi

mkdir -p ./done

for f in $@; do
	echo "`date`: submitting $f"
	tile_submitter -l 5 -t 25000 -C $BASE_DIR/etc/dqueue.conf -f $f
	mv $f ./done
	echo "`date`: $f submitted"
done 
