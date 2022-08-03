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

#ifndef SRC_TINT_UTILS_HASHSET_H_
#define SRC_TINT_UTILS_HASHSET_H_

#include <stddef.h>
#include <algorithm>
#include <functional>
#include <tuple>
#include <utility>

#include "src/tint/utils/vector.h"

namespace tint::utils {

/// An unordered set that uses a robin-hood hashing algorithm.
/// @see the fantastic tutorial: https://programming.guide/robin-hood-hashing.html
template <typename T, size_t N = 8, typename HASH = std::hash<T>, typename EQUAL = std::equal_to<T>>
class Hashset {
  public:
    /// Type of `T`.
    using value_type = T;
    /// Value of `N`
    static constexpr size_t static_length = N;

    /// Constructor
    Hashset() { slots_.Resize(N); }

    /// Copy constructor
    /// @param other the other Hashset to copy
    Hashset(const Hashset& other) : count_(other.count_) {
        slots_.Resize(other.slots_.Length());
        for (size_t i = 0; i < slots_.Length(); i++) {
            auto& src = other.slots_[i];
            if (src.alive) {
                auto& dst = slots_[i];
                new (&dst.Value()) T(src.Value());
                dst.alive = true;
                dst.hash = src.hash;
            }
        }
    }

    /// Move constructor
    /// @param other the other Hashset to move
    Hashset(Hashset&& other) : count_(other.count_) {
        slots_.Resize(other.slots_.Length());
        for (size_t i = 0; i < slots_.Length(); i++) {
            auto& src = other.slots_[i];
            if (src.alive) {
                auto& dst = slots_[i];
                new (&dst.Value()) T(std::move(src.Value()));
                dst.alive = true;
                dst.hash = src.hash;
            }
        }
    }

    /// Destructor
    ~Hashset() {
        for (auto& slot : slots_) {
            if (slot.alive) {
                slot.Value().~T();
            }
        }
    }

    /// Adds a value to the set.
    /// @param value the value to add to the set.
    /// @param replace if true, then any existing entries that are equal to `value` are replaced
    ///        with `value`. If false, then Add() will do nothing if the set already contains an
    ///        entry with the same value.
    /// @returns true if `value` was added to the set, or if `value` replaced an existing entry.
    template <typename V>
    bool Add(V&& value, bool replace = false) {
        if (ShouldRehash(count_ + 1)) {
            Reserve(slots_.Length() * 2);
        }
        const auto [index, hash] = IndexAndHash(value);
        auto& slot = slots_[index];
        if (!slot.alive) {
            new (&slot.Value()) T(std::forward<V>(value));
            slot.alive = true;
            slot.hash = hash;
            count_++;
            return true;
        } else {
            return Insert(std::forward<V>(value), hash, index, replace);
        }
    }

    /// Removes an entry from the set.
    /// @param value the value to remove from the set.
    /// @returns true if an entry was removed.
    template <typename V>
    bool Remove(const V& value) {
        const auto [found, start] = IndexOf(value);
        if (!found) {
            return false;
        }

        T* last = nullptr;

        Probe(start, [&](Slot& slot, size_t, size_t) {
            if (last) {
                if (slot.distance == 0) {
                    return Action::Stop;
                }
                new (last) T(std::move(slot.Value()));
            }

            slot.Value().~T();
            slot.alive = false;
            slot.hash = 0;
            slot.distance = 0;
            last = &slot.Value();
            return Action::Continue;
        });

        count_--;
        return true;
    }

    /// @param value the value to search for.
    /// @returns a pointer to the entry that is equal to the given value, or nullptr if the set does
    ///          not contain the given value.
    template <typename V>
    const T* Find(const V& value) const {
        const auto [found, index] = IndexOf(value);
        return found ? &slots_[index].Value() : nullptr;
    }

    /// Checks whether an entry exists in the set
    /// @param value the value to search for.
    /// @returns true if the set contains an entry with the given value.
    template <typename V>
    bool Contains(const V& value) const {
        const auto [found, _] = IndexOf(value);
        return found;
    }

    /// Pre-allocates memory so that the set can hold at least `new_capacity` entries.
    /// @param new_capacity the new capacity of the set.
    void Reserve(size_t new_capacity) {
        const size_t num_slots = (new_capacity * kRehashFactor) / 100;
        if (slots_.Length() >= num_slots) {
            return;
        }

        Vector<T, N> values;
        values.Reserve(count_);
        for (auto& slot : slots_) {
            if (slot.alive) {
                values.Push(std::move(slot.Value()));
                slot.Value().~T();
            }
        }

        count_ = 0;
        slots_.Clear();
        slots_.Resize(num_slots);

        for (auto& value : values) {
            Add(std::move(value));
        }
    }

    /// @returns the number of entries in the set.
    size_t Count() const { return count_; }

  private:
    static constexpr size_t kRehashFactor = 150;  // percent

    template <typename V>
    std::tuple<size_t, size_t> IndexAndHash(const V& value) const {
        const size_t hash = HASH()(value);
        const size_t index = Wrap(hash);
        return {index, hash};
    }

    template <typename V>
    std::tuple<bool, size_t> IndexOf(const V& value) const {
        const auto idx_hash = IndexAndHash(value);
        const auto start = std::get<0>(idx_hash);
        const auto hash = std::get<1>(idx_hash);

        bool found = false;
        size_t idx = 0;

        Probe(start, [&](const Slot& slot, size_t distance, size_t index) {
            if (!slot.alive) {
                return Action::Stop;
            }
            if (slot.Equals(hash, value)) {
                found = true;
                idx = index;
                return Action::Stop;
            }
            if (slot.distance < distance) {
                return Action::Stop;
            }
            return Action::Continue;
        });

        return {found, idx};
    }

    bool Insert(T value, size_t hash, size_t start, bool replace) {
        bool inserted = false;
        Probe(start, [&](Slot& slot, size_t distance, size_t) {
            if (slot.alive) {
                if (slot.Equals(hash, value)) {
                    if (replace) {
                        slot.Value() = value;
                        inserted = true;
                    }
                    return Action::Stop;
                }
                if (slot.distance < distance) {
                    std::swap(value, slot.Value());
                    std::swap(hash, slot.hash);
                    std::swap(distance, slot.distance);
                }
            } else {
                new (&slot.Value()) T(std::move(value));
                slot.alive = true;
                slot.hash = hash;
                slot.distance = distance;
                count_++;
                inserted = true;
                return Action::Stop;
            }
            return Action::Continue;
        });

        return inserted;
    }

    template <typename F>
    void Probe(size_t start, F&& f) const {
        size_t index = start;
        for (size_t distance = 0; distance < slots_.Length(); distance++) {
            if (f(slots_[index], distance, index) == Action::Stop) {
                return;
            }
            index = Wrap(index + 1);
        }
    }

    template <typename F>
    void Probe(size_t start, F&& f) {
        size_t index = start;
        for (size_t distance = 0; distance < slots_.Length(); distance++) {
            if (f(slots_[index], distance, index) == Action::Stop) {
                return;
            }
            index = Wrap(index + 1);
        }
    }

    enum class Action { Continue, Stop };

    bool ShouldRehash(size_t count) const {
        return ((count * kRehashFactor) / 100) > slots_.Length();
    }

    size_t Wrap(size_t index) const { return index % slots_.Length(); }

    static constexpr size_t kAlignment = std::max(alignof(T), alignof(size_t));

    struct alignas(kAlignment) Slot {
        T& Value() { return *Bitcast<T*>(&data[0]); }
        const T& Value() const { return *Bitcast<const T*>(&data[0]); }

        template <typename V>
        bool Equals(size_t value_hash, const V& value) const {
            return value_hash == hash && EQUAL()(value, Value());
        }

        uint8_t data[sizeof(T)];
        size_t hash = 0;
        size_t distance = 0;
        bool alive = false;
    };

    Vector<Slot, N> slots_;
    size_t count_ = 0;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_HASHSET_H_
