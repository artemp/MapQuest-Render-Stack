""" Simple expiry information server built on Twisted. """

#
# expiry information is kept on a metatile basis, per-style 
# without regard for the format or position within a 
# metatile. this is kept in a mmapped file which is 
# periodically flushed in case of node failure.
#

import logging
import struct
import datetime
import sys
import os
import mmap
import traceback
from twisted.internet.protocol import Factory, Protocol, DatagramProtocol
from twisted.internet import reactor, task
from ConfigParser import SafeConfigParser
from math import sqrt

# very basic logging - TODO: make this much better!
log = logging

# how long between flushes to disk (in seconds)
FLUSH_PERIOD = 5

# maximum z that's possible within a 64-bit unsigned 
# number, which is what we transmit across the network
MAX_Z = 35

class ZLevels():
    """Stores sizes, in number of metatiles, at each zoom 
    level. This allows metatiles to be looked up in a 
    single file or linear array without the need for 
    multiple files which would complicate syncing to disk.
    """
    
    def __init__(self):
        z_sizes = map(lambda z: 4**max(0,z-3), range(0,MAX_Z))
        self.offsets = reduce(lambda s,i: s + [s[-1] + i], z_sizes, [0])

    def size_of(self, max_z):
        bit_size = self.offsets[max_z + 1]
        return bit_size / 8 + (1 if bit_size % 8 > 0 else 0)

class Info():
    """In-memory statistics collection object. This keeps the
    information about how long gets and puts have taken and is
    able to calculate and return statistics about the history
    of those statistics.
    """

    def __init__(self, max_z, filename):
        # build array of sizes
        self.zlevels = ZLevels()

        # try and open the file
        try:
            wanted_size = self.zlevels.size_of(max_z)
            file_size = 0 if not os.path.isfile(filename) else os.path.getsize(filename)

            # note: only expand the file
            if wanted_size > file_size:
                log.info("Expanding %s to %d bytes" % (repr(filename), wanted_size))
                with open(filename, "a+b") as f:
                    with open("/dev/zero", "rb") as fzero:
                        f.write(fzero.read(wanted_size - file_size))

            self.file = open(filename, "r+b")
            # mmap the file
            self.mmap = mmap.mmap(self.file.fileno(), 0)
            # record the max size
            self.max_idx = self.mmap.size() * 8

        except Exception as e:
            log.error("Error setting up memory-mapped file at %s: %s" % (str(filename), str(e)))
            raise

    def set_bit(self, idx, val):
        """Set the bit at @idx to @val."""
        if idx < self.max_idx:
            self.mmap.seek(idx / 8)
            byte = struct.unpack('@B', self.mmap.read_byte())[0]
            if val > 0:
                byte = byte | (1 << (idx % 8))
            else:
                byte = byte & ~(1 << (idx % 8))
            self.mmap.seek(idx / 8)
            self.mmap.write_byte(struct.pack('@B', byte))

    def get_bit(self, idx):
        """Return the value of the bit at @idx, or None if it's 
        out of range."""
        if idx < self.max_idx:
            self.mmap.seek(idx / 8)
            byte = struct.unpack('@B', self.mmap.read_byte())[0]
            bit = (byte >> (idx % 8)) & 1
            return bit
        else:
            return None

    def flush(self):
        """Flush the mmapped file to disk so that it's relatively
        safe from unexpected shutdowns.
        """
        os.fsync(self.file.fileno())

    def shutdown(self):
        """Try to shut down cleanly."""
        self.flush()
        

class InfoFlusher:
    """Callable to periodically flush the information to disk
    to prevent a crash from completely wiping out the data.
    """
    def __init__(self, infos):
        self.infos = infos

    def __call__(self):
        for info in self.infos.itervalues():
            info.flush()

class InfoUDP(DatagramProtocol):
    """Expiry information collector and provider over UDP. This
    is an extremely light-weight protocol providing the absolute
    minimum amount of information about the 
    """

    def __init__(self, infos):
        self.infos = infos

    def datagramReceived(self, datagram, address):
        try:
            idx, val, cmd, style = struct.unpack('!Qbc255p', datagram)

            if style in self.infos:
                info = self.infos[style]

                if cmd == "S":
                    info.set_bit(idx, val)
                    self.transport.write("OK", address)
                elif cmd == "G":
                    bit = info.get_bit(idx)
                    self.transport.write(struct.pack('!B', bit), address)
                else:
                    self.transport.write("ERR", address)

            else:
                self.transport.write("ERR", address)


        except Exception as e:
            log.error("Error receiving info packet from %s: %s" % (str(address), str(e)))
            log.error("Traceback: %s" % traceback.format_exc())
            # send an error response
            self.transport.write("ERR", address)

class InfoGuard:
    """A guard to help the system shut down cleanly."""

    def __init__(self, max_z, directory):
        self.max_z = max_z
        self.directory = directory
        self.infos = dict()
    
    def __enter__(self):
        for name in os.listdir(self.directory):
            filename = os.path.join(self.directory, name)
            if os.path.isfile(filename):
                info = Info(self.max_z, filename)
                self.infos[name] = info
        return self

    def __exit__(self, type, value, traceback):
        for info in self.infos.itervalues():
            info.shutdown()

    def __getitem__(self, style):
        if style in self.infos:
            return self.infos[style]
        else:
            info = Info(self.max_z, os.path.join(self.directory, style))
            self.infos[style] = info
            return info

    def __contains__(self, item):
        # can contain anything that's a valid file name
        return '/' not in str(item)

    def itervalues(self):
        return self.infos.itervalues()

def main(config):
    port = int(config.get('app:main', 'expiry_info.port'))
    max_z = int(config.get('app:main', 'expiry_info.max_z'))
    directory = config.get('app:main', 'expiry_info.directory')

    # try and make the directory for style info, if it 
    # doesn't already exist.
    if not os.path.isdir(directory):
        os.makedirs(directory)

    with InfoGuard(max_z, directory) as infos:

        reactor.listenUDP(port, InfoUDP(infos))

        flusher = task.LoopingCall(InfoFlusher(infos))
        flusher.start(FLUSH_PERIOD)

        log.info("Running expiry information server on port %d" % port)
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

