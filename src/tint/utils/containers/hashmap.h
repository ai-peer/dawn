// Copyright 2022 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_UTILS_CONTAINERS_HASHMAP_H_
#define SRC_TINT_UTILS_CONTAINERS_HASHMAP_H_

#include <functional>
#include <optional>
#include <utility>

#include "src/tint/utils/containers/hashmap_base.h"
#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/ice/ice.h"
#include "src/tint/utils/math/hash.h"

namespace tint {

/// HashmapEntry is a key-value pair.
template <typename KEY, typename VALUE>
struct HashmapEntry {
    /// The key type
    using Key = KEY;
    /// The value type
    using Value = VALUE;

    static const Key& KeyOf(const HashmapEntry& entry) { return entry.key; }

    /// The key
    Key key;

    /// The value
    Value value;
};

template <typename K1, typename V1, typename K2, typename V2>
inline static bool operator==(const HashmapEntry<K1, V1>& lhs, const HashmapEntry<K2, V2>& rhs) {
    return lhs.key == rhs.key && lhs.value == rhs.value;
}

/// Writes the HashmapEntry to the stream.
/// @param out the stream to write to
/// @param key_value the HashmapEntry to write
/// @returns out so calls can be chained
template <typename STREAM,
          typename KEY,
          typename VALUE,
          typename = traits::EnableIfIsOStream<STREAM>>
auto& operator<<(STREAM& out, const HashmapEntry<KEY, VALUE>& key_value) {
    return out << "[" << key_value.key << ": " << key_value.value << "]";
}

/// The return value of Hashmap::Get(Key)
template <typename T>
struct GetResult {
    /// The value found in the map, or null if the entry was not found.
    T* value = nullptr;

    /// @returns `true` if #value is not null.
    operator bool() const { return value; }

    /// @returns the dereferenced value, which must not be null.
    T& operator*() const { return *value; }

    /// @returns the dereferenced value, which must not be null.
    T* operator->() const { return value; }

    /// @param other the value to compare against the object that #value points to.
    /// @returns true if #value is not null and the object that #value points to is equal to @p
    /// other.
    template <typename O>
    bool operator==(const O& other) const {
        return value && *value == other;
    }

    /// @param other the value to compare against the object that #value points to.
    /// @returns true if #value is null or the object that #value points to is not equal to @p
    /// other.
    template <typename O>
    bool operator!=(const O& other) const {
        return !value || *value != other;
    }
};

/// The return value of Hashmap::Add(Key, Value)
template <typename T>
struct AddResult {
    /// The value of the entry with the given key.
    /// If an existing entry was found with the key, then this is the value of the existing entry,
    /// otherwise the value of the newly inserted entry.
    T& value;

    /// True if an entry did not already exist in the map with the given key.
    bool added = false;

    /// @returns #added
    operator bool() const { return added; }
};

/// An unordered hashmap, with a fixed-size capacity that avoids heap allocations.
template <typename KEY,
          typename VALUE,
          size_t N,
          typename HASH = Hasher<KEY>,
          typename EQUAL = EqualTo<KEY>>
class Hashmap : public HashmapBase<HashmapEntry<HashmapKey<KEY, HASH, EQUAL>, VALUE>, N> {
    using Base = HashmapBase<HashmapEntry<HashmapKey<KEY, HASH, EQUAL>, VALUE>, N>;

  public:
    /// The key type
    using Key = typename Base::Key;
    /// The value type
    using Value = VALUE;
    /// The key-value type for a map entry
    using Entry = HashmapEntry<Key, Value>;

    /// Add attempts to insert a new entry into the map.
    /// If an existing entry exists with the given key, then the entry is not replaced.
    /// @param key the new entry's key
    /// @param value the new entry's value
    /// @return an AddResult.
    template <typename K, typename V>
    AddResult<Value> Add(K&& key, V&& value) {
        auto res = this->Put(
            /* replace */ false, std::forward<K>(key), std::forward<V>(value));
        return {res.entry.value, res.action == Base::PutResult::kAdded};
    }

    /// Inserts a new entry into the map or updates an existing entry.
    /// @param key the new entry's key
    /// @param value the new entry's value
    template <typename K, typename V>
    void Replace(K&& key, V&& value) {
        this->Put(/* replace */ true, std::forward<K>(key), std::forward<V>(value));
    }

    /// Searches for an entry with the given key value returning that entry if found, otherwise
    /// returns null.
    /// @param key the entry's key value to search for.
    /// @returns the entry, if found otherwise null
    template <typename K>
    Entry* GetEntry(K&& key) {
        const auto [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return &node->Entry();
        }
        return nullptr;
    }

    /// Searches for an entry with the given key value returning that entry if found, otherwise
    /// returns null.
    /// @param key the entry's key value to search for.
    /// @returns the entry, if found otherwise null
    template <typename K>
    const Entry* GetEntry(K&& key) const {
        const auto [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return &node->Entry();
        }
        return nullptr;
    }

    template <typename K>
    GetResult<Value> Get(K&& key) {
        const auto [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return {&node->Entry().value};
        }
        return {nullptr};
    }

    template <typename K>
    GetResult<const Value> Get(K&& key) const {
        const auto [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return {&node->Entry().value};
        }
        return {nullptr};
    }

    /// Searches for an entry with the given key value returning that value if found, otherwise
    /// returns @p not_found.
    /// @param key the entry's key value to search for.
    /// @returns the value of the entry, if found otherwise @p not_found.
    template <typename K>
    const Value& Get(K&& key, const Value& not_found) const {
        const auto [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return node->Entry().value;
        }
        return not_found;
    }

    /// Searches for an entry with the given key, adding an entry with @p key the result of calling
    /// @p create if the entry was not found.
    /// @note: Before calling `create`, the map will insert a zero-initialized value for the given
    /// key, which will be replaced with the value returned by @p create. If @p create adds an entry
    /// with @p key to this map, it will be replaced.
    /// @param key the entry's key value to search for.
    /// @param create the create function to call if the map does not contain the key.
    /// @returns the entry.
    template <typename K, typename CREATE>
    Entry& GetOrCreateEntry(K&& key, CREATE&& create) {
        this->GrowIfNeeded();
        const auto [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return node->Entry();
        }
        auto& entry = this->Insert(slot_idx, Key{hash, std::forward<K>(key)}, Value{})->Entry();
        entry.value = create();
        return entry;
    }

    /// Searches for an entry with the given key value, adding and returning a newly created
    /// zero-initialized value if the entry was not found.
    /// @param key the entry's key value to search for.
    /// @returns the value of the entry.
    template <typename K>
    Entry& GetOrZeroEntry(K&& key) {
        this->GrowIfNeeded();
        auto const [hash, slot_idx] = this->HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return node->Entry();
        }
        return this->Insert(slot_idx, Key{hash, std::forward<K>(key)}, Value{})->Entry();
    }

    /// Searches for an entry with the given key, adding and returning the result of calling
    /// @p create if the entry was not found.
    /// @note: Before calling `create`, the map will insert a zero-initialized value for the given
    /// key, which will be replaced with the value returned by @p create. If @p create adds an entry
    /// with @p key to this map, it will be replaced.
    /// @param key the entry's key value to search for.
    /// @param create the create function to call if the map does not contain the key.
    /// @returns the value of the entry.
    template <typename K, typename CREATE>
    Value& GetOrCreate(K&& key, CREATE&& create) {
        return GetOrCreateEntry(std::forward<K>(key), std::forward<CREATE>(create)).value;
    }

    /// Searches for an entry with the given key value, adding and returning a newly created
    /// zero-initialized value if the entry was not found.
    /// @param key the entry's key value to search for.
    /// @returns the value of the entry.
    template <typename K>
    Value& GetOrZero(K&& key) {
        return GetOrZeroEntry(std::forward<K>(key)).value;
    }

    /// @returns the keys of the map as a vector.
    /// @note the order of the returned vector is non-deterministic between compilers.
    template <size_t N2 = N>
    Vector<KEY, N2> Keys() const {
        Vector<KEY, N2> out;
        out.Reserve(this->Count());
        for (auto& it : *this) {
            out.Push(it.key.Value());
        }
        return out;
    }

    /// @returns the values of the map as a vector
    /// @note the order of the returned vector is non-deterministic between compilers.
    template <size_t N2 = N>
    Vector<Value, N2> Values() const {
        Vector<Value, N2> out;
        out.Reserve(this->Count());
        for (auto& it : *this) {
            out.Push(it.value);
        }
        return out;
    }

    /// Equality operator
    /// @param other the other Hashmap to compare this Hashmap to
    /// @returns true if this Hashmap has the same key and value pairs as @p other
    template <typename K, typename V, size_t N2>
    bool operator==(const Hashmap<K, V, N2>& other) const {
        if (this->Count() != other.Count()) {
            return false;
        }
        for (auto& it : *this) {
            auto other_val = other.Get(it.key.Value());
            if (!other_val || it.value != *other_val) {
                return false;
            }
        }
        return true;
    }

    /// Inequality operator
    /// @param other the other Hashmap to compare this Hashmap to
    /// @returns false if this Hashmap has the same key and value pairs as @p other
    template <typename K, typename V, size_t N2>
    bool operator!=(const Hashmap<K, V, N2>& other) const {
        return !(*this == other);
    }
};

/// Hasher specialization for Hashmap
template <typename K, typename V, size_t N, typename HASH, typename EQUAL>
struct Hasher<Hashmap<K, V, N, HASH, EQUAL>> {
    /// @param map the Hashmap to hash
    /// @returns a hash of the map
    size_t operator()(const Hashmap<K, V, N, HASH, EQUAL>& map) const {
        auto hash = Hash(map.Count());
        for (auto it : map) {
            // Use an XOR to ensure that the non-deterministic ordering of the map still produces
            // the same hash value for the same entries.
            hash ^= Hash(it.key.Value(), it.value);
        }
        return hash;
    }
};

/// Writes the Hashmap to the stream.
/// @param out the stream to write to
/// @param map the Hashmap to write
/// @returns out so calls can be chained
template <typename STREAM,
          typename KEY,
          typename VALUE,
          size_t N,
          typename HASH,
          typename EQUAL,
          typename = traits::EnableIfIsOStream<STREAM>>
auto& operator<<(STREAM& out, const Hashmap<KEY, VALUE, N, HASH, EQUAL>& map) {
    out << "Hashmap{";
    bool first = true;
    for (auto it : map) {
        if (!first) {
            out << ", ";
        }
        first = false;
        out << it;
    }
    out << "}";
    return out;
}

}  // namespace tint

#endif  // SRC_TINT_UTILS_CONTAINERS_HASHMAP_H_
