#!/bin/bash
ps -ef | grep workerNFS.py | grep -v grep | awk '{print $2}' | xargs kill -9
ps -ef | grep start_nfs_worker.sh | grep -v grep | awk '{print $2}' | xargs kill -9
