#!/bin/bash
ps -ef | grep workerHSS.py | grep -v grep | awk '{print $2}' | xargs kill -9
ps -ef | grep start_hss_worker.sh | grep -v grep | awk '{print $2}' | xargs kill -9
