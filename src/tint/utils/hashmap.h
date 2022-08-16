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

#include "src/tint/utils/hash.h"
#include "src/tint/utils/hashset.h"

namespace tint::utils {

/// An unordered map that uses a robin-hood hashing algorithm.
/// @see the fantastic tutorial: https://programming.guide/robin-hood-hashing.html
template <typename K,
          typename V,
          size_t N,
          typename HASH = Hasher<K>,
          typename EQUAL = std::equal_to<K>>
class Hashmap {
    template <typename CREATE>
    struct LazyCreator {
        const K& key;
        CREATE create;
    };

    struct Entry {
        Entry(K k, V v) : key(std::move(k)), value(std::move(v)) {}
        Entry(const Entry&) = default;
        Entry(Entry&&) = default;

        template <typename CREATE>
        Entry(const LazyCreator<CREATE>& creator)  // NOLINT(runtime/explicit)
            : key(creator.key), value(creator.create()) {}

        template <typename CREATE>
        Entry& operator=(LazyCreator<CREATE>&& creator) {
            key = std::move(creator.key);
            value = creator.create();
            return *this;
        }

        Entry& operator=(const Entry&) = default;
        Entry& operator=(Entry&&) = default;

        K key;
        V value;
    };

    struct Hasher {
        size_t operator()(const Entry& entry) const { return HASH()(entry.key); }
        size_t operator()(const K& key) const { return HASH()(key); }
        template <typename CREATE>
        size_t operator()(const LazyCreator<CREATE>& lc) const {
            return HASH()(lc.key);
        }
    };
    struct Equality {
        size_t operator()(const Entry& a, const Entry& b) const { return EQUAL()(a.key, b.key); }
        size_t operator()(const K& a, const Entry& b) const { return EQUAL()(a, b.key); }
        template <typename CREATE>
        size_t operator()(const LazyCreator<CREATE>& lc, const Entry& b) const {
            return EQUAL()(lc.key, b.key);
        }
    };

    using Set = Hashset<Entry, N, Hasher, Equality>;

  public:
    /// A Key and Value const-reference pair.
    struct KeyValue {
        /// key of a map entry
        const K& key;
        /// value of a map entry
        const V& value;
    };

    /// Iterator for the map
    class Iterator {
      public:
        /// @returns the key of the entry pointed to by this iterator
        const K& Key() const { return it->key; }

        /// @returns the value of the entry pointed to by this iterator
        const V& Value() const { return it->value; }

        /// Increments the iterator
        /// @returns this iterator
        Iterator& operator++() {
            ++it;
            return *this;
        }

        /// Equality operator
        /// @param other the other iterator to compare this iterator to
        /// @returns true if this iterator is equal to other
        bool operator==(const Iterator& other) const { return it == other.it; }

        /// Inequality operator
        /// @param other the other iterator to compare this iterator to
        /// @returns true if this iterator is not equal to other
        bool operator!=(const Iterator& other) const { return it != other.it; }

        /// @returns a pair of key and value for the entry pointed to by this iterator
        KeyValue operator*() const { return {Key(), Value()}; }

      private:
        /// Friend class
        friend class Hashmap;

        /// Underlying iterator type
        using SetIterator = typename Set::Iterator;

        explicit Iterator(SetIterator i) : it(i) {}

        SetIterator it;
    };

    /// Removes all entries from the map.
    void Clear() { set_.Clear(); }

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

    /// Searches for an entry with the given key value, adding and returning the result of
    /// calling `create` if the entry was not found.
    /// @param key the entry's key value to search for.
    /// @param create the create function to call if the map does not contain the key.
    /// @returns the value of the entry.
    template <typename CREATE>
    V& GetOrCreate(const K& key, CREATE&& create) {
        LazyCreator<CREATE> lc{key, std::forward<CREATE>(create)};
        auto res = set_.template Insert</* replace */ false>(std::move(lc));
        return res.entry->value;
    }

    /// Searches for an entry with the given key value, adding and returning a newly created
    /// zero-initialized value if the entry was not found.
    /// @param key the entry's key value to search for.
    /// @returns the value of the entry.
    V& GetOrZero(const K& key) {
        auto zero = [] { return V{}; };
        LazyCreator<decltype(zero)> lc{key, zero};
        auto res = set_.template Insert</* replace */ false>(std::move(lc));
        return res.entry->value;
    }

    /// Searches for an entry with the given key value.
    /// @param key the entry's key value to search for.
    /// @returns the a pointer to the value of the entry with the given key, or nullptr if the entry
    /// was not found.
    /// @warning the pointer must not be used after the map is mutated
    V* Find(const K& key) {
        if (auto* entry = set_.Find(key)) {
            return &entry->value;
        }
        return nullptr;
    }

    /// Searches for an entry with the given key value.
    /// @param key the entry's key value to search for.
    /// @returns the a pointer to the value of the entry with the given key, or nullptr if the entry
    /// was not found.
    /// @warning the pointer must not be used after the map is mutated
    const V* Find(const K& key) const {
        if (auto* entry = set_.Find(key)) {
            return &entry->value;
        }
        return nullptr;
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

    /// @returns true if the map contains no entries.
    bool IsEmpty() const { return set_.IsEmpty(); }

    /// @returns an iterator to the start of the map
    Iterator begin() { return Iterator{set_.begin()}; }

    /// @returns an iterator to the end of the map
    Iterator end() { return Iterator{set_.end()}; }

  private:
    Set set_;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_HASHMAP_H_
