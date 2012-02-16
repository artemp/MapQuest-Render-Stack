#!/bin/bash

E_BADARGS=65
source /data/mapquest/render_stack/scripts/env.sh
PREFIX="/data/mapquest/render_stack"
export PYTHONPATH="$PREFIX/bin/gcc-4.1.2/release"
WORKER="$PREFIX/py/workerNFS.py"
WORKER_CONFIG="/data/mapquest/render_stack/conf/nfs_worker.conf"
QUEUE_CONFIG="/data/mapquest/render_stack/conf/pgqueue.conf"
LOGDIR="/srv/planet/logs/render_stack/worker"

if [ ! -n "$1" ]
then
  echo "Usage: `basename $0` <worker_id>"
  exit $E_BADARGS
fi  

echo "starting worker # $1... "
HOSTNAME=`hostname`
while [ 1 ]; do
    echo "Loop starting worker at --> $(date)"
    python $WORKER $WORKER_CONFIG $QUEUE_CONFIG $HOSTNAME-nfs-$1 &> $LOGDIR/$HOSTNAME-nfs-$1.log
    echo "worker stopped" 
    sleep 60
done
#nohup python $WORKER $WORKER_CONFIG $QUEUE_CONFIG $HOSTNAME-hss-$ID &> $LOGDIR/$HOSTNAME-hss-$1.log &
