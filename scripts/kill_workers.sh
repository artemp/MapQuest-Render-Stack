#!/bin/bash
ps -ef | grep worker.py | grep pg | grep -v grep | awk '{print $2}' | xargs kill -9
