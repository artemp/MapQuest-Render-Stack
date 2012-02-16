import os
import socket
import struct
import logging
import sys
from threading import Lock

# maximum z that's possible within a 64-bit unsigned 
# number, which is what we transmit across the network
MAX_Z = 35

# size of a metatile
METATILE = 8

log = logging.getLogger(__name__)

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

    def tile_to_meta_idx(self, x, y, z):
        morton_code = (self._interleave(x) << 1) | self._interleave(y)
        return self.offsets[z] + morton_code

    def _interleave(self, n):
        """Bit interleaving function, i.e: turns 11011 into
        1010001010 and is used to create Morton codes. This
        extended version is able to handle up to 2^32-1.
        """
        n&= 0xffffffff
        n = (n | (n << 16)) & 0x0000FFFF0000FFFF
        n = (n | (n <<  8)) & 0x00FF00FF00FF00FF
        n = (n | (n <<  4)) & 0x0F0F0F0F0F0F0F0F
        n = (n | (n <<  2)) & 0x3333333333333333
        n = (n | (n <<  1)) & 0x5555555555555555
        return n
    
    def _uninterleave(self, n):
        """Inverse of bit interleaving function, able to 
        handle 64-bit numbers allowing outputs to be up to
        2^32-1 in size."""
        n&= 0x5555555555555555
        n = (n ^ (n >>  1)) & 0x3333333333333333
        n = (n ^ (n >>  2)) & 0x0f0f0f0f0f0f0f0f
        n = (n ^ (n >>  4)) & 0x00ff00ff00ff00ff
        n = (n ^ (n >>  8)) & 0x0000ffff0000ffff
        n = (n ^ (n >> 16)) & 0xffffffff
        return n

class mqExpiryInfo:
    """Client class for expiry information. Contacts a server
    over UDP to get metatile expiry information.
    """

    def __init__(self, host, port):
        # set up the socket for the first time
        self.sock = None
        self._socket_init()

        family, socktype, proto, canonname, sockaddr = socket.getaddrinfo(host, port, socket.AF_INET)[0]
        self.sockaddr = sockaddr
        # offsets structure for turning metatile locations into
        # raw 64-bit integers.
        self.zlevels = ZLevels()
        # a lock, so we can ensure thread-local access to the
        # socket.
        self.mutex = Lock()

    def _socket_init(self):
        """Initialise (or re-initialise) the socket. Note that
        unless you're in the constructor, you'll need to be holding
        the mutex when you call this method.
        """
        # shutdown the socket if it already exists
        if self.sock is not None:
            log.info("Re-opening socket. Old socket is %s [%s]" % (str(self.sock), str(self.sock.fileno())))
            try:
                self.sock.shutdown(socket.SHUT_RDWR)
            except:
                # this fails if the socket isn't considered connected, which
                # is annoying but perhaps ignorable?
                pass

            try:
                self.sock.close()
            except:
                # this may also throw an error if the close fails. but we're
                # going to open a new socket anyway, so maybe ignorable?
                pass

            # get rid of reference to old socket, will be garbage collected.
            self.sock = None

        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        log.info("Opening socket. Socket is %s [%s]" % (str(self.sock), str(self.sock.fileno())))
        # set up so that sockets timeout after 0.2s - should
        # be enough on localhost to receive a reply.
        self.sock.settimeout(0.2)

    def get_tile(self, x, y, z, style):
        """Gets information about expiry from the server. Returns
        true if the metatile is expired, false otherwise and None
        if an error occurred."""
        idx = self.zlevels.tile_to_meta_idx(x / METATILE, y / METATILE, z)
        rep = self._get_bit(idx, str(style))
        if (rep == 'ERR') or (rep is None):
            return None
        else:
            bit = struct.unpack('@B', rep)[0]
            return bit != 0

    def set_tile(self, x, y, z, style, val):
        """Sends information about the expiry to the server.
        Returns true if the request succeeded and false otherwise.
        """
        idx = self.zlevels.tile_to_meta_idx(x / METATILE, y / METATILE, z)
        bit = 0
        if val == True:
            bit = 1
        ret = self._set_bit(idx, bit, str(style))
        return ret == 'OK'

    def _basic_req(self, msg):
        reply = None

        with self.mutex:
            try:
                self.sock.sendto(msg, self.sockaddr)
                reply = self.sock.recv(4096)
            except:
                # if this times out, return none so that other code
                # can handle the error via fall-backs. reset the socket
                # so that no odd trailing packets will be received and
                # misinterpreted.
                self._socket_init()
                log.error("Error talking to expiry info server: %s" % str(sys.exc_info()))
                reply = None

        return reply
    
    def _get_bit(self, idx, style):
        return self._basic_req(struct.pack('!Qbc255p', idx, 0, 'G', style))

    def _set_bit(self, idx, bit, style):
        return self._basic_req(struct.pack('!Qbc255p', idx, bit, 'S', style))
