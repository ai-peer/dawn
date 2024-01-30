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

template <typename T>
struct GetResult {
    T* value = nullptr;

    operator bool() const { return value; }
    T& operator*() const { return *value; }
    T* operator->() const { return value; }

    template <typename O>
    bool operator==(const O& other) const {
        return value && *value == other;
    }

    template <typename O>
    bool operator!=(const O& other) const {
        return !value || *value != other;
    }
};

/// An unordered map that uses a robin-hood hashing algorithm.
template <typename KEY,
          typename VALUE,
          size_t N,
          typename HASH = Hasher<KEY>,
          typename EQUAL = EqualTo<KEY>>
class Hashmap : public HashmapBase<KEY, VALUE, N, HASH, EQUAL> {
    using Base = HashmapBase<KEY, VALUE, N, HASH, EQUAL>;

    template <typename T>
    using ReferenceKeyType = traits::CharArrayToCharPtr<std::remove_reference_t<T>>;

  public:
    /// The key type
    using Key = KEY;
    /// The value type
    using Value = VALUE;
    /// The key-value type for a map entry
    using Entry = KeyValue<Key, Value>;

    struct AddResult {
        Value& value;
        bool added = false;
        operator bool() const { return added; }
    };

    template <typename K, typename V>
    AddResult Add(K&& key, V&& value) {
        auto res = this->Put(/* replace */ false, std::forward<K>(key), std::forward<V>(value));
        return {res.entry.value, res.action == Base::PutResult::kAdded};
    }

    template <typename K, typename V>
    bool Replace(K&& key, V&& value) {
        return this->Put(/* replace */ true, std::forward<K>(key), std::forward<V>(value)).action ==
               Base::PutResult::kReplaced;
    }

    template <typename K>
    GetResult<Value> Get(K&& key) {
        auto const [hash, slot_idx] = this->Hash(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return {&node->Entry().value};
        }
        return {nullptr};
    }

    template <typename K>
    GetResult<const Value> Get(K&& key) const {
        auto const [hash, slot_idx] = this->Hash(key);
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
        auto const [hash, slot_idx] = this->Hash(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return node->Entry().value;
        }
        return not_found;
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
        this->GrowIfNeeded();
        auto const [hash, slot_idx] = this->Hash(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return node->Entry().value;
        }
        auto& value = this->Insert(hash, slot_idx, std::forward<K>(key), Value{})->Entry().value;
        value = create();
        return value;
    }

    /// Searches for an entry with the given key value, adding and returning a newly created
    /// zero-initialized value if the entry was not found.
    /// @param key the entry's key value to search for.
    /// @returns the value of the entry.
    template <typename K>
    Value& GetOrZero(K&& key) {
        this->GrowIfNeeded();
        auto const [hash, slot_idx] = this->Hash(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return node->Entry().value;
        }
        return this->Insert(hash, slot_idx, std::forward<K>(key), Value{})->Entry().value;
    }

    /// @returns the keys of the map as a vector.
    /// @note the order of the returned vector is non-deterministic between compilers.
    template <size_t N2 = N>
    Vector<Key, N2> Keys() const {
        Vector<Key, N2> out;
        out.Reserve(this->Count());
        for (auto it : *this) {
            out.Push(it.key);
        }
        return out;
    }

    /// @returns the values of the map as a vector
    /// @note the order of the returned vector is non-deterministic between compilers.
    template <size_t N2 = N>
    Vector<Value, N2> Values() const {
        Vector<Value, N2> out;
        out.Reserve(this->Count());
        for (auto it : *this) {
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
        for (auto it : *this) {
            auto other_val = other.Get(it.key);
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
            hash ^= Hash(it.key, it.value);
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
