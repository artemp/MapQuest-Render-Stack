#include "fake_tile.hpp"
#include <stdexcept>
#include <cstdio>

using std::runtime_error;

fake_tile::fake_tile(int x, int y, int z, int formats) {
   total_size = 1;
   size_t content_offset = 0;
   for (int fmt = 0; fmt < 30; ++fmt) {
      if (formats & (1 << fmt)) {
         // 16 bytes for a zz|xxxxxx|yyyyyy format message.
         total_size += sizeof(meta_tile) + 64 * 16;
         content_offset += sizeof(meta_tile);
      }
   }

   ptr = (char *)malloc(total_size);
   if (ptr == 0) { throw runtime_error("Can't allocate."); }
   
   size_t header_count = 0;
   for (int fmt = 0; fmt < 30; ++fmt) {
      if (formats & (1 << fmt)) {
         meta_tile *meta = (meta_tile *)(ptr + header_count * sizeof(meta_tile));
         meta->magic[0] = 'M';
         meta->magic[1] = 'E';
         meta->magic[2] = 'T';
         meta->magic[3] = 'A';
         meta->n_entries = 64;
         meta->x = x;
         meta->y = y;
         meta->z = z;
         meta->format = 1 << fmt;
         for (int dx = 0; dx < 8; ++dx) {
            for (int dy = 0; dy < 8; ++dy) {
               char *str = ptr + content_offset;
               snprintf(str, 17, "%03d|%06d|%06d", z, x+dx, y+dy);
               meta->entries[8 * dy + dx].offset = content_offset;
               meta->entries[8 * dy + dx].len = 16;
               content_offset += 16;
            }
         }
         ++header_count;
      }
   }
}

fake_tile::~fake_tile() {
   if (ptr) { free(ptr); }
}
   
