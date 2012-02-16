#!/bin/bash

E_BADARGS=65
source /data/mapquest/render_stack/scripts/env.sh
PREFIX="/data/mapquest/render_stack"
export PYTHONPATH="$PREFIX/bin/gcc-4.1.2/release"
WORKER="$PREFIX/py/worker.py"
LOGDIR="/srv/planet/logs/render_stack/worker"

if [ ! -n "$1" ]
then
  echo "Usage: `basename $0` <num_workers>"
  exit $E_BADARGS
fi  

echo "starting $1 workers... "
HOSTNAME=`hostname`
for ((ID=1; ID <= $1 ; ID++))
do
	nohup /data/mapquest/render_stack/scripts/start_nfs_worker.sh $ID > $LOGDIR/loop-$ID.stdout 2> $LOGDIR/loop-$ID.stderr & 
#	nohup python $WORKER $WORKER_CONFIG $QUEUE_CONFIG $HOSTNAME-hss-$ID &> $LOGDIR/$HOSTNAME-hss-$ID.log &
done
