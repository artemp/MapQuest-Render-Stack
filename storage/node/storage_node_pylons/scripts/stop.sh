#!/bin/sh

export PATH=/data/mapquest/stdbase/apache2.2/bin:/data/mapquest/stdbase/bin:$PATH
export LD_LIBRARY_PATH=/data/mapquest/stdbase/lib:/data/mapquest/stdbase/lib64:$LD_LIBRARY_PATH

#set up paths
export PATH=/data/mapquest/stdbase/apache2.2/bin:/data/mapquest/stdbase/bin:$PATH
export LD_LIBRARY_PATH=/data/mapquest/stdbase/lib:/data/mapquest/stdbase/lib64:$LD_LIBRARY_PATH

#stop request handlers
#apache is started/stopped/restarted with monit, for example: http://mq-tile-lm10.ihost.aol.com:2812/
#httpd -k stop -f /data/mapquest/storage_node_pylons/conf/httpd.conf

#stop the monitoring/stats process
if [ -e /data/mapquest/storage_node_pylons/pids/stats.pid ]; then
	pid=`cat /data/mapquest/storage_node_pylons/pids/stats.pid`
	#kill it if its running
	running=`pgrep python | grep $pid | wc -l`
	if [ $running -gt 0 ]; then
		kill -9 $pid
	else
		echo "PID for stats/monitoring no longer running"
	fi
	rm -f /data/mapquest/storage_node_pylons/pids/stats.pid
else
	echo "PID file for stats/monitoring not found, assumed not running"
fi
#stop the expiry process
if [ -e /data/mapquest/storage_node_pylons/pids/expiry.pid ]; then
        pid=`cat /data/mapquest/storage_node_pylons/pids/expiry.pid`
        #kill it if its running
        running=`pgrep python | grep $pid | wc -l`
        if [ $running -gt 0 ]; then
                kill -9 $pid
        else
                echo "PID for expiry no longer running"
        fi
        rm -f /data/mapquest/storage_node_pylons/pids/expiry.pid
else
        echo "PID file for expiry not found, assumed not running"
fi
