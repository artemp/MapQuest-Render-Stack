###
import os
import sys
import signal

#for NMS logging library
import mq_logging

def handler(signum, frame):
    raise KeyboardInterrupt

class Watcher:    
    def __init__(self):
        self.child = os.fork()
        if self.child == 0:
            return
        else:
            self.watch()

    def watch(self):
        try:
            signal.signal(signal.SIGTERM, handler)
            os.wait()
        except KeyboardInterrupt:
            mq_logging.info('KeyBoardInterrupt')
            self.kill()
        sys.exit()

    def kill(self):
        try:
            os.kill(self.child, signal.SIGKILL)
        except OSError: pass
