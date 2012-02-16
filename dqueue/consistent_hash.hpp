/*------------------------------------------------------------------------------
 *
 *  This file is part of rendermq  
 *
 *  Author: matt.amos@mapquest.com
 *
 *  Copyright 2010-1 Mapquest, Inc.  All Rights reserved.
 *
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *-----------------------------------------------------------------------------*/

#ifndef CONSISTENT_HASH_HPP
#define CONSISTENT_HASH_HPP

#include <boost/functional/hash.hpp>
#include <boost/optional.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <map>
#include <iostream>
#include <memory>

namespace rendermq {

namespace hash_helper {
// used to fix up differences in size_t size between machines.
template <typename int_type>
int_type get_value(boost::mt19937 &rng); 
}

/**
 * consistent hash implementation.
 *
 * this maps key_types to value_types on a pointer-sized ring of hash values.
 * the key_type should be the type that's used to look up a value, in this 
 * case the metatile details. the value type should be the type that is being
 * looked-to, in this case the broker's identity.
 */
template <typename K_t, typename V_t>
class consistent_hash {
public:
  // type of the underlying data structure
  typedef K_t key_type;
  typedef V_t value_type;
  typedef std::map<size_t, value_type> ring_t;

  // construct empty consistent hash ring with the given number of 
  // repeats for each entry.
  consistent_hash(int repeats_)
    : repeats(repeats_) {
  }

  // adds a value_type to the hash ring
  void insert(const value_type &val) {
    const size_t vh = val_hash(val);
    rng.seed(vh);
    for (int i = 0; i < repeats; ++i) {
      size_t x = hash_helper::get_value<size_t>(rng);
      ring.insert(std::make_pair(x, val));
    }
  }

  // removes all value_types from the ring
  void erase(const value_type &val) {
    const size_t vh = val_hash(val);
    rng.seed(vh);
    for (int i = 0; i < repeats; ++i) {
      size_t x = hash_helper::get_value<size_t>(rng);
      ring.erase(x);
    }
  }

  // looks up a value_type from corresponding key_type
  boost::optional<value_type> lookup(const key_type &k) const {
    const size_t h = shuffle(key_hash(k));
    typename ring_t::const_iterator itr = ring.lower_bound(h);

    // if the result is past the end of the linear map, wrap it around
    // to the beginning to form a ring.
    if (itr == ring.end()) itr = ring.begin();

    // if it's still at the end, then we're empty
    if (itr == ring.end()) {
      return boost::optional<value_type>();

    } else {
      return boost::optional<value_type>(itr->second);
    }
  }

  void print(std::ostream &out) const {
    size_t x = 0;
    for (typename ring_t::const_iterator itr = ring.begin();
         itr != ring.end(); ++itr) {
      out << itr->first << " => " << itr->second << " (" << itr->first - x << ")\n";
      x = itr->first;
    }
  }

private:
  // number of times each value is mapped onto the ring
  const int repeats;

  // the ring of hashes as a std::map - this isn't circular, but that's
  // easily simulated.
  ring_t ring;

  // functor to hash keys and values
  boost::hash<key_type> key_hash;
  boost::hash<value_type> val_hash;

  /* used to generate a series of pseudo-random points along the ring
   * for each value. because this is a PRNG, and the whole point of them
   * is that successive values shouldn't be correlated, this should 
   * guarantee good distribution of ring entries through space. */
  boost::mt19937 rng;

  /* shuffles the bits in a 64-bit number according to Wang's algorithm, 
   * detailed here http://www.concentric.net/~ttwang/tech/inthash.htm
   * this should result in an even distribution of bits throughout the
   * 64-bit space, making it more likely that low-order bit correlations
   * in the input won't cause adjacent keys to map to the same value. */
  inline uint64_t shuffle(uint64_t key) const {
    key = (~key) + (key << 21);
    key = key ^ ((key >> 24) | (key << 8));
    key = (key + (key << 3)) + (key << 8);
    key = key ^ ((key >> 14) | (key << 18));
    key = (key + (key << 2)) + (key << 4);
    key = key ^ ((key >> 28) | (key << 4));
    key = key + (key << 31);
    return key;
  }
};

} // namespace rendermq

#endif /* CONSISTENT_HASH_HPP */
