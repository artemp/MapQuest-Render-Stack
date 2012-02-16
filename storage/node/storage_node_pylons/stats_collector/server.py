""" Simple stats logging server built on Twisted. """

import logging
import struct
import datetime
import json
import sys
from twisted.internet.protocol import Factory, Protocol, DatagramProtocol
from twisted.internet import reactor, task
from ConfigParser import SafeConfigParser
from math import sqrt

# very basic logging - TODO: make this much better!
log = logging

# how long between flushes to clear out old data (in seconds)
FLUSH_PERIOD = 5 * 60

# how long to keep data in memory for
KEEP_PERIOD = datetime.timedelta(minutes = 60)

class Stats():
    """In-memory statistics collection object. This keeps the
    information about how long gets and puts have taken and is
    able to calculate and return statistics about the history
    of those statistics.
    """

    def __init__(self):
        self.stats = { 'g': list(), 'p': list() }

    def add(self, table, usec):
        """Add a new entry into the statistics."""
        now = datetime.datetime.now()
        self.stats[table].append((now, usec))

    def flush(self):
        """Drop any statistics older than KEEP_PERIOD to stop
        memory usage getting too high.
        """
        now = datetime.datetime.now()
        keep_time = now - KEEP_PERIOD
        for k, v in self.stats.iteritems():
            self.stats[k] = [x for x in v if x[0] >= keep_time]

    def make(self, table, time, t0):
        """Make statistics about a particular table over some 
        time period.
        """
        n, a, q = 0, 0.0, 0.0
        for t, x in self.stats[table]:
            if t > t0:
                n = n + 1
                an = a + (x - a) / float(n)
                q = q + (x - an) * (x - a)
                a = an
        if n > 1:
            return { 'time': time, 'n': n, 'avg': a, 'dev': sqrt(q / float(n - 1)) }
        else:
            return { 'time': time, 'n': n, 'avg': a, 'dev': 0.0 }

class StatsFlusher:
    """Callable to periodically flush the statistics to prevent
    them from building up in memory.
    """
    def __init__(self, stats):
        self.stats = stats

    def __call__(self):
        self.stats.flush()

class StatsUDP(DatagramProtocol):
    """Statistics collector over UDP. This is a very low-overhead
    method of getting these statistics, as it is much more light- 
    weight than a TCP connection.
    """

    def __init__(self, stats):
        self.stats = stats

    def datagramReceived(self, datagram, address):
        try:
            table, usec = struct.unpack('!cI', datagram)
            self.stats.add(table, usec)

        except Exception as e:
            log.error("Error receiving stats packet from %s: %s" % (str(address), str(e)))

class StatsTCP(Protocol):
    """Statistics provider over TCP. When the socket is connected,
    the server just dumps the statistics as a JSON string over the
    socket.
    """
    def connectionMade(self):
        now = datetime.datetime.now()
        stats = self.factory.stats
        data = { 'gets' : [ stats.make('g', 'now', now - datetime.timedelta(seconds=5)),
                            stats.make('g', '5min', now - datetime.timedelta(minutes=5)),
                            stats.make('g', 'hour', now - datetime.timedelta(minutes=60)) ],
                 'posts' : [ stats.make('p', 'now', now - datetime.timedelta(seconds=5)),
                             stats.make('p', '5min', now - datetime.timedelta(minutes=5)),
                             stats.make('p', 'hour', now - datetime.timedelta(minutes=60)) ] }
        self.transport.write(json.dumps(data))
        self.transport.write('\r\n')
        self.transport.loseConnection()

class StatsFactory(Factory):
    protocol = StatsTCP

    def __init__(self, stats):
        self.stats = stats

def main(config):
    port = int(config.get('app:main', 'stats_collector.port'))

    stats = Stats()

    reactor.listenUDP(port, StatsUDP(stats))
    reactor.listenTCP(port, StatsFactory(stats))

    flusher = task.LoopingCall(StatsFlusher(stats))
    flusher.start(FLUSH_PERIOD)

    print "Running stats collection server on port %d" % port
    reactor.run()

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print>>sys.stderr,"Usage: server.py <config file>"
        print>>sys.stderr,"NOTE: use the config file from the storage node (e.g: development.ini)."
        sys.exit(1)

    try:
        config = SafeConfigParser()
        config.read(sys.argv[1])
        main(config)

    except Exception as e:
        print>>sys.stderr,"ERROR: %s" % str(e)

