// Copyright 2022 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TINT_UTILS_HASHMAP_H_
#define SRC_TINT_UTILS_HASHMAP_H_

#include <functional>
#include <optional>
#include <utility>

#include "src/tint/utils/hashset.h"

namespace tint::utils {

/// An unordered map that uses a robin-hood hashing algorithm.
/// @see the fantastic tutorial: https://programming.guide/robin-hood-hashing.html
template <typename K,
          typename V,
          size_t N = 8,
          typename HASH = std::hash<K>,
          typename EQUAL = std::equal_to<K>>
class Hashmap {
  public:
    /// Adds the key-value pair to the map.
    /// @param key the entry's key to add to the map
    /// @param value the entry's value to add to the map
    /// @param replace if true, then any existing entries that have a key equal to `key` are
    ///        replaced with the new entry. If false, then Add() will do nothing if the map already
    ///        contains an entry with the same value.
    /// @returns true if the entry was added to the set, or if an existing entry was replaced.
    template <typename KEY, typename VALUE>
    bool Add(KEY&& key, VALUE&& value, bool replace = false) {
        return set_.Add(Entry{std::forward<KEY>(key), std::forward<VALUE>(value)}, replace);
    }

    /// Searches for an entry with the given key value.
    /// @param key the entry's key value to search for.
    /// @returns the value of the entry with the given key, or no value if the entry was not found.
    std::optional<V> Get(const K& key) {
        if (auto* entry = set_.Find(key)) {
            return entry->value;
        }
        return {};
    }

    /// Removes an entry from the set with a key equal to `key`.
    /// @param key the entry key value to remove.
    /// @returns true if an entry was removed.
    bool Remove(const K& key) { return set_.Remove(key); }

    /// Checks whether an entry exists in the map with a key equal to `key`.
    /// @param key the entry key value to search for.
    /// @returns true if the map contains an entry with the given key.
    bool Contains(const K& key) const { return set_.Contains(key); }

    /// Pre-allocates memory so that the map can hold at least `new_capacity` entries.
    /// @param new_capacity the new capacity of the map.
    void Reserve(size_t new_capacity) { set_.Reserve(new_capacity); }

    /// @returns the number of entries in the map.
    size_t Count() const { return set_.Count(); }

  private:
    struct Entry {
        K key;
        V value;
    };
    struct Hasher {
        size_t operator()(const Entry& entry) const { return HASH()(entry.key); }
        size_t operator()(const K& key) const { return HASH()(key); }
    };
    struct Equality {
        size_t operator()(const Entry& a, const Entry& b) const { return EQUAL()(a.key, b.key); }
        size_t operator()(const K& a, const Entry& b) const { return EQUAL()(a, b.key); }
    };
    Hashset<Entry, N, Hasher, Equality> set_;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_HASHMAP_H_
