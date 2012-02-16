import unittest
import sys

class Collector:
    def __init__(self):
        self.children = dict()
        self.terminals = list()

    def add(self, path):
        if len(path) > 1:
            x = path.pop(0)
            if x not in self.children:
                self.children[x] = Collector()
            self.children[x].add(path)
        else:
            self.terminals.append(path[0])

    def entries(self):
        return len(self.terminals) + len(self.children)

    def check(self, limit):
        if self.entries() > limit:
            raise Exception("More than %d entries in this directory." % limit)
        for x in self.children.values():
            x.check(limit)

class MqCacheTests(unittest.TestCase):
    def _makeCache(self, root):
        from storage_node_pylons.lib.mqCache import mqCache
        return mqCache(root)

    def dirs_z(self, z):
        from storage_node_pylons.lib.mqTile import mqTile
        cache = self._makeCache("/")
        fs = Collector()
        x = 0
        for y in range(2**z):
            tile = mqTile("foo", z, x, y, "jpg", "vx", "1")
            path = cache.construct_name(tile)
            segs = path.split("/")
            fs.add(segs)
        fs.check(1000)

    def test_dirs(self):
        for z in [10, 14, 18]:
            self.dirs_z(z)

if __name__ == "__main__":
    sys.path.append("../")
    unittest.main()
