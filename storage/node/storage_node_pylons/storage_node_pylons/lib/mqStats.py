import os
import logging
import socket
import stat
import datetime
import struct
import json
from math import sqrt
from threading import Lock

log = logging.getLogger(__name__)

class mqStats():
    """ Class for keeping statistics about requests across all python threads 
        (and possibly processes) on this machine. This is currently implemented
        by a separate "collector" process with which the processing threads
        communicate over sockets.
    """

    def __init__(self, host, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.mutex = Lock()
        family, socktype, proto, canonname, sockaddr = socket.getaddrinfo(host, port, socket.AF_INET)[0]
        log.debug("sockaddr = %s" % repr(sockaddr))
        self.sockaddr = sockaddr

    def update_get(self, usec):
        """Updates the statistics with a GET and the given number of
           milliseconds that the operation took.
        """
        self.stat_inc('gets', usec)

    def update_post(self, usec):
        """Updates the statistics with a POST and the given number of
           milliseconds that the operation took.
        """
        self.stat_inc('posts', usec)

    def stat_inc(self, table_name, usec):
        """Updates the named table with an entry and the given number of
           milliseconds.
        """
        try:
            msg = struct.pack( '!cI', table_name[0], int(usec) ) 
            with self.mutex:
                self.sock.sendto( msg, self.sockaddr )

        except Exception as e:
            log.error("Can't update stats: %s" % str(e))
            raise e

    def get_stats(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(self.sockaddr)
            total_data = []
            while True:
                data = sock.recv(8192)
                if not data: break
                total_data.append(data)
            sock.close()
            return json.loads(''.join(total_data))

        except Exception as e:
            log.error("Unable to read stats from DB: %s" % str(e))
            raise e
