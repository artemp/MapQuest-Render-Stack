#!/bin/bash
ps -ef | grep worker.py | grep -v grep | awk '{print $2}' | xargs kill -9
