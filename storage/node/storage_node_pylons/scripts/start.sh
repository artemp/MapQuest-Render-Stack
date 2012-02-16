#!/bin/sh

#set up paths
export PATH=/data/mapquest/stdbase/apache2.2/bin:/data/mapquest/stdbase/bin:$PATH
export LD_LIBRARY_PATH=/data/mapquest/stdbase/lib:/data/mapquest/stdbase/lib64:$LD_LIBRARY_PATH

BASE_DIR=/data/mapquest/storage_node_pylons

#start the monitoring/stats process
nohup python ${BASE_DIR}/stats_collector/server.py ${BASE_DIR}/conf/production.ini > /data/mapquest/logs/stats.log & echo $! > ${BASE_DIR}/pids/stats.pid

# start the expiry information process
nohup python ${BASE_DIR}/expiry_info/server.py ${BASE_DIR}/conf/production.ini > /data/mapquest/logs/expiry.log & echo $! > ${BASE_DIR}/pids/expiry.pid

#start up request handlers
#apache is started/stopped/restarted with monit, for example: http://mq-tile-lm10.ihost.aol.com:2812/
#httpd -k start -f ${BASE_DIR}/conf/httpd.conf

