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
#include <optional>
#include <tuple>
#include <utility>

#include "src/tint/debug.h"
#include "src/tint/utils/vector.h"

namespace tint::utils {

/// Action taken by Hashset::Insert()
enum class InsertAction {
    /// Insert() added a new entry to the Hashset
    kAdded,
    /// Insert() replaced an existing entry in the Hashset
    kReplaced,
    /// Insert() found an existing entry, which was not replaced.
    kFoundExisting,
};

/// An unordered set that uses a robin-hood hashing algorithm.
/// @see the fantastic tutorial: https://programming.guide/robin-hood-hashing.html
template <typename T, size_t N, typename HASH = std::hash<T>, typename EQUAL = std::equal_to<T>>
class Hashset {
    /// A slot is in single entry in the underlying vector.
    /// A slot can either be empty or filled with a value. If the slot is empty, #hash and #distance
    /// will be zero.
    struct Slot {
        template <typename V>
        bool Equals(size_t value_hash, const V& val) const {
            return value_hash == hash && EQUAL()(val, value.value());
        }

        /// The slot value. If this does not contain a value, then the slot is vacant.
        std::optional<T> value;
        /// The precomputed hash of value.
        size_t hash = 0;
        size_t distance = 0;
    };

    static constexpr size_t kRehashFactor = 150;  // percent
    static constexpr size_t kNumFixedSlots = (N * kRehashFactor) / 100;
    static constexpr size_t kMinSlots = std::max<size_t>(kNumFixedSlots, 4);

  public:
    /// Iterator for the set
    class Iterator {
      public:
        /// @returns the value pointed to by this iterator
        const T* operator->() const { return &current->value.value(); }

        /// Increments the iterator
        /// @returns this iterator
        Iterator& operator++() {
            if (current == end) {
                return *this;
            }
            current++;
            SkipToNextValue();
            return *this;
        }

        /// Equality operator
        /// @param other the other iterator to compare this iterator to
        /// @returns true if this iterator is equal to other
        bool operator==(const Iterator& other) const { return current == other.current; }

        /// Inequality operator
        /// @param other the other iterator to compare this iterator to
        /// @returns true if this iterator is not equal to other
        bool operator!=(const Iterator& other) const { return current != other.current; }

        /// @returns a reference to the value at the iterator
        const T& operator*() const { return current->Value(); }

      private:
        /// Friend class
        friend class Hashset;

        Iterator(Slot* c, Slot* e) : current(c), end(e) { SkipToNextValue(); }

        /// Moves the iterator forward, stopping at the next slot that is not empty.
        void SkipToNextValue() {
            while (current != end && !current->value.has_value()) {
                current++;
            }
        }

        Slot* current;
        Slot* end;
    };

    /// Type of `T`.
    using value_type = T;
    /// Value of `N`
    static constexpr size_t static_length = N;

    /// Constructor
    Hashset() { slots_.Resize(kMinSlots); }

    /// Copy constructor
    /// @param other the other Hashset to copy
    Hashset(const Hashset& other) = default;

    /// Move constructor
    /// @param other the other Hashset to move
    Hashset(Hashset&& other) = default;

    /// Destructor
    ~Hashset() { Clear(); }

    /// Copy-assignment operator
    /// @param other the other Hashset to copy
    /// @returns this so calls can be chained
    Hashset& operator=(const Hashset& other) = default;

    /// Move-assignment operator
    /// @param other the other Hashset to move
    /// @returns this so calls can be chained
    Hashset& operator=(Hashset&& other) = default;

    /// Removes all entries from the set.
    void Clear() {
        slots_.Clear();
        slots_.Resize(kMinSlots);
        count_ = 0;
    }

    /// Adds a value to the set.
    /// @param value the value to add to the set.
    /// @param replace if true, then any existing entries that are equal to `value` are replaced
    ///        with `value`. If false, then Add() will do nothing if the set already contains an
    ///        entry with the same value.
    /// @returns true if `value` was added to the set, or if `value` replaced an existing entry.
    template <typename V>
    bool Add(V&& value, bool replace = false) {
        if (replace) {
            return Insert<true>(std::forward<V>(value)).action != InsertAction::kFoundExisting;
        } else {
            return Insert<false>(std::forward<V>(value)).action != InsertAction::kFoundExisting;
        }
    }

    /// Result of Insert()
    struct InsertionResult {
        /// Whether the insert replaced or added a new entry to the set.
        InsertAction action = InsertAction::kAdded;
        /// A pointer to the inserted element.
        /// @warning do not modify this pointer in a way that would cause the equality of hash of
        ///          the entry to change. Doing this will corrupt the Hashset.
        T* entry = nullptr;
    };

    /// Adds a value to the set.
    /// @param value the value to add to the set.
    /// @tparam REPLACE if true, then any existing entries that are equal to `value` are replaced
    ///         with `value`. If false, then Insert() will do nothing if the set already contains an
    ///         entry with the same value.
    /// @returns A InsertionResult describing the result of the insertion
    /// @warning do not modify the inserted element in a way that would cause the equality of hash
    ///          of the entry to change. Doing this will corrupt the Hashset.
    template <bool REPLACE, typename V>
    InsertionResult Insert(V&& value) {
        if (ShouldRehash(count_ + 1)) {
            Reserve(slots_.Length() * 2);
        }
        const auto idx_hash = IndexAndHash(value);
        const auto start = std::get<0>(idx_hash);
        auto hash = std::get<1>(idx_hash);

        InsertionResult result{};
        Walk(start, [&](Slot& slot, size_t distance, size_t index) {
            if (!slot.value.has_value()) {
                // Found an empty slot.
                // Place value directly into the slot, and we're done.
                slot.value.emplace(std::forward<V>(value));
                slot.hash = hash;
                slot.distance = distance;
                count_++;
                result = InsertionResult{InsertAction::kAdded, &slot.value.value()};
                return Action::Stop;
            }

            // Slot has an entry

            if (slot.Equals(hash, value)) {
                // Slot is equal to value. Replace or preserve?
                if constexpr (REPLACE) {
                    slot.value = std::forward<V>(value);
                    result = InsertionResult{InsertAction::kReplaced, &slot.value.value()};
                } else {
                    result = InsertionResult{InsertAction::kFoundExisting, &slot.value.value()};
                }
                return Action::Stop;
            }

            if (slot.distance < distance) {
                // Existing slot has a closer distance than the value we're attempting to insert.
                // Steal from the rich!
                // Move value into a temporary, and swap the value, hash and distance with slot.
                T temp{std::forward<V>(value)};
                std::swap(temp, slot.value.value());
                std::swap(hash, slot.hash);
                std::swap(distance, slot.distance);

                // Find a new home for the evicted slot.
                InsertShuffle(Wrap(index + 1), std::move(temp), hash, distance + 1);

                count_++;
                result = InsertionResult{InsertAction::kAdded, &slot.value.value()};

                return Action::Stop;
            }
            return Action::Continue;
        });

        return result;
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

        // Shuffle the entries backwards until we either find a free slot, or a slot that has zero
        // distance.
        Walk(start, [&](Slot& slot, size_t, size_t index) {
            Slot& next = slots_[Wrap(index + 1)];
            if (next.distance == 0) {
                // note: `distance == 0` also includes empty slots.
                // Erase this slot, and stop shuffling.
                slot = {};
                return Action::Stop;
            } else {
                // Shuffle the next slot backwards into this slot.
                slot.value = std::move(next.value);
                slot.hash = next.hash;
                slot.distance = next.distance - 1;
            }
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
        return found ? &slots_[index].value.value() : nullptr;
    }

    /// @param value the value to search for.
    /// @returns a pointer to the entry that is equal to the given value, or nullptr if the set does
    ///          not contain the given value.
    /// @warning do not modify the inserted element in a way that would cause the equality of hash
    ///          of the entry to change. Doing this will corrupt the Hashset.
    template <typename V>
    T* Find(const V& value) {
        const auto [found, index] = IndexOf(value);
        return found ? &slots_[index].value.value() : nullptr;
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
        const size_t num_slots = std::max((new_capacity * kRehashFactor) / 100, kMinSlots);
        if (slots_.Length() >= num_slots) {
            return;
        }

        Vector<T, N> values;
        values.Reserve(count_);
        for (auto& slot : slots_) {
            if (slot.value.has_value()) {
                values.Push(std::move(slot.value.value()));
            }
        }

        Clear();
        slots_.Resize(num_slots);

        for (auto& value : values) {
            Add(std::move(value));
        }
    }

    /// @returns the number of entries in the set.
    size_t Count() const { return count_; }

    /// @returns true if the set contains no entries.
    bool IsEmpty() const { return count_ == 0; }

    /// @returns an iterator to the start of the map
    Iterator begin() { return {slots_.begin(), slots_.end()}; }

    /// @returns an iterator to the end of the map
    Iterator end() { return {slots_.end(), slots_.end()}; }

    /// A debug function for checking that the set is in good health.
    /// Asserts if the set is corrupted.
    void ValidateIntegrity() const {
        size_t num_alive = 0;
        for (size_t slot_idx = 0; slot_idx < slots_.Length(); slot_idx++) {
            const auto& slot = slots_[slot_idx];
            if (slot.value.has_value()) {
                num_alive++;
                auto const [index, hash] = IndexAndHash(slot.value.value());
                TINT_ASSERT(Utils, hash == slot.hash);
                TINT_ASSERT(Utils, slot_idx == Wrap(index + slot.distance));
            }
        }
        TINT_ASSERT(Utils, num_alive == count_);
    }

  private:
    enum class Action { Continue, Stop };

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

        Walk(start, [&](const Slot& slot, size_t distance, size_t index) {
            if (!slot.value.has_value()) {
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

    template <typename F>
    void Walk(size_t start, F&& f) const {
        size_t index = start;
        for (size_t distance = 0; distance < slots_.Length(); distance++) {
            if (f(slots_[index], distance, index) == Action::Stop) {
                return;
            }
            index = Wrap(index + 1);
        }
        tint::diag::List diags;
        TINT_ICE(Utils, diags) << "Hashset::Walk() looped entire set without finding a slot";
    }

    template <typename F>
    void Walk(size_t start, F&& f) {
        size_t index = start;
        for (size_t distance = 0; distance < slots_.Length(); distance++) {
            if (f(slots_[index], distance, index) == Action::Stop) {
                return;
            }
            index = Wrap(index + 1);
        }
        tint::diag::List diags;
        TINT_ICE(Utils, diags) << "Hashset::Walk() looped entire set without finding a slot";
    }

    void InsertShuffle(size_t start, T value, size_t hash, size_t distance) {
        Walk(start, [&](Slot& slot, size_t, size_t) {
            if (!slot.value.has_value()) {
                slot.value.emplace(std::move(value));
                slot.hash = hash;
                slot.distance = distance;
                return Action::Stop;
            }

            if (slot.distance < distance) {
                std::swap(value, slot.value.value());
                std::swap(hash, slot.hash);
                std::swap(distance, slot.distance);
            }
            distance++;
            return Action::Continue;
        });
    }

    bool ShouldRehash(size_t count) const {
        return ((count * kRehashFactor) / 100) > slots_.Length();
    }

    size_t Wrap(size_t index) const { return index % slots_.Length(); }

    Vector<Slot, kNumFixedSlots> slots_;
    size_t count_ = 0;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_HASHSET_H_
