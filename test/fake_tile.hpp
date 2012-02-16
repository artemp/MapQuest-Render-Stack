#ifndef TEST_FAKE_TILE_HPP
#define TEST_FAKE_TILE_HPP

#include <boost/array.hpp>

/* utility classes for creating 'fake' meta tiles to return, so that there
 * is data present and the broker won't consider it a failure. */
struct meta_entry {
   int offset, len;
};

struct meta_tile {
   char magic[4];
   int n_entries, x, y, z, format;
   boost::array<meta_entry, 64> entries;
};

struct fake_tile {
   fake_tile(int x, int y, int z, int formats);
   ~fake_tile();
   
   char *ptr;
   size_t total_size;
};

#endif /* TEST_FAKE_TILE_HPP */
