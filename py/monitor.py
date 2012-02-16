#!/usr/bin/env python

import zmq
import sys,os,errno,signal
from math import pi,cos,sin,log,exp,atan
from watcher import Watcher

if __name__ == "__main__" :
    Watcher()
    if len(sys.argv) !=3 :
        print>>sys.stderr,"Usage: %s <address> <command>" % sys.argv[0]
        sys.exit(1)
    
    ctx = zmq.Context()
    client = ctx.socket(zmq.REQ)
    client.connect(sys.argv[1])
    request = sys.argv[2]
    client.send(request,zmq.NOBLOCK)
    print>>sys.stderr,client.recv()
    
    
            
