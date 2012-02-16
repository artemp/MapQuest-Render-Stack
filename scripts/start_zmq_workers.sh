#!/bin/bash

E_BADARGS=65
PREFIX="/mqdata/render_stack/rendermq"
export PYTHONPATH="$PREFIX/bin/gcc-4.1.2/release"
WORKER="$PREFIX/py/worker.py"
WORKER_CONFIG="/srv/planet/config/worker.conf"
QUEUE_CONFIG="/srv/planet/config/dqueue.conf"
LOGDIR="/srv/planet/mongrel2/logs"

if [ ! -n "$1" ]
then
  echo "Usage: `basename $0` <num_workers>"
  exit $E_BADARGS
fi  

echo "starting $1 workers... "
HOSTNAME=`hostname`
for ((ID=1; ID <= $1 ; ID++))
do
    nohup $WORKER $WORKER_CONFIG $QUEUE_CONFIG $HOSTNAME-$ID &> $LOGDIR/$HOSTNAME-$ID.log &
done




