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

#ifndef SRC_TINT_UTILS_CONTAINERS_HASHMAP_BASE_H_
#define SRC_TINT_UTILS_CONTAINERS_HASHMAP_BASE_H_

#include <algorithm>
#include <functional>
#include <optional>
#include <tuple>
#include <utility>

#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/ice/ice.h"
#include "src/tint/utils/math/hash.h"
#include "src/tint/utils/traits/traits.h"

namespace tint {

template <typename T, typename HASH = Hasher<T>, typename EQUAL = std::equal_to<T>>
class HashmapKey {
    T value_;

  public:
    using Key = HashmapKey<T, HASH, EQUAL>;
    using Hash = HASH;
    using Equal = EQUAL;

    static const Key& KeyOf(const Key& key) { return key; }

    HashmapKey(size_t hash_, const T& value) : value_(value), hash(hash_) {}

    HashmapKey(size_t hash_, T&& value) : value_(std::forward<T>(value)), hash(hash_) {}

    HashmapKey(const T& value) : value_(value), hash(HASH{}(value_)) {}

    HashmapKey(T&& value) : value_(std::forward<T>(value)), hash(HASH{}(value_)) {}

    HashmapKey(const HashmapKey&) = default;

    HashmapKey(HashmapKey&&) = default;

    ~HashmapKey() = default;

    HashmapKey& operator=(const HashmapKey& other) {
        TINT_ASSERT(hash == other.hash);
        value_ = other.Value();
        return *this;
    }

    HashmapKey& operator=(HashmapKey&& other) {
        TINT_ASSERT(hash == other.hash);
        value_ = std::move(other.Value());
        return *this;
    }

    bool operator==(const HashmapKey& other) const {
        return hash == other.hash && EQUAL{}(value_, other.Value());
    }

    template <typename RHS>
    bool operator==(const RHS& other) const {
        return hash == HASH{}(other) && EQUAL{}(value_, other);
    }

    /// @returns the value of the key
    const T& Value() const { return value_; }

    /// @returns the value of the key
    operator const T&() const { return value_; }

    /// @returns the pointer to the value, or the value itself if T is a pointer.
    auto operator->() const {
        if constexpr (std::is_pointer_v<T>) {
            // operator-> is useless if the T is a pointer, so automatically unwrap a pointer.
            return value_;
        } else {
            return &value_;
        }
    }

    /// The hash of value
    const size_t hash;
};

/// Writes the HashmapKey to the stream.
/// @param out the stream to write to
/// @param key the HashmapKey to write
/// @returns out so calls can be chained
template <typename STREAM, typename T, typename = traits::EnableIfIsOStream<STREAM>>
auto& operator<<(STREAM& out, const HashmapKey<T>& key) {
    if constexpr (traits::HasOperatorShiftLeft<STREAM, T>) {
        return out << key.Value();
    } else {
        return out << "<hashmap-key>";
    }
}

template <typename ENTRY, size_t N>
class HashmapBase {
  protected:
    struct Node;

  public:
    using Entry = ENTRY;
    using Key = typename Entry::Key;
    using Hash = typename Key::Hash;
    using Equal = typename Key::Equal;

    /// The minimum capacity of the hashmap.
    static constexpr size_t kMinCapacity = std::max<size_t>(N, 8);

    /// The capacity / number of slots.
    static constexpr size_t kLoadFactor = 75;

    /// @returns the target slot vector size to hold `count` map entries.
    static constexpr size_t NumSlots(size_t count) {
        return (std::max<size_t>(count, kMinCapacity) * kLoadFactor) / 100;
    }

    HashmapBase() {
        slots_.Resize(slots_.Capacity());
        for (auto& node : fixed_) {
            AddToFree(&node);
        }
    }

    HashmapBase(const HashmapBase& other) : HashmapBase() {
        if (&other != this) {
            Copy(other);
        }
    }

    HashmapBase(HashmapBase&& other) : HashmapBase() {
        if (&other != this) {
            Move(std::move(other));
        }
    }

    ~HashmapBase() {
        for (size_t slot_idx = 0; slot_idx < slots_.Length(); slot_idx++) {
            auto* node = slots_[slot_idx];
            while (node) {
                auto next = node->next;
                node->Destroy();
                node = next;
            }
        }
        auto* allocation = allocations_;
        while (allocation) {
            auto* next = allocation->next;
            free(allocation);
            allocation = next;
        }
    }

    HashmapBase& operator=(const HashmapBase& other) {
        if (&other != this) {
            Clear();
            Copy(other);
        }
        return *this;
    }

    HashmapBase& operator=(HashmapBase&& other) {
        if (&other != this) {
            Clear();
            Move(std::move(other));
        }
        return *this;
    }

    size_t Count() const { return count_; }

    bool IsEmpty() const { return count_ == 0; }

    void Clear() {
        for (size_t slot_idx = 0; slot_idx < slots_.Length(); slot_idx++) {
            auto* node = slots_[slot_idx];
            while (node) {
                auto next = node->next;
                node->Destroy();
                AddToFree(node);
                node = next;
            }
            slots_[slot_idx] = nullptr;
        }
    }

    void Reserve(size_t n) {
        if (n > capacity_) {
            AllocateNodes(n - capacity_);
        }
    }

    template <typename K = Key>
    bool Contains(K&& key) const {
        const auto [hash, slot_idx] = HashAndSlot(key);
        if (auto* node = this->FindNode(hash, slot_idx, key)) {
            return true;
        }
        return false;
    }

    template <typename K = Key>
    bool Remove(K&& key) {
        const auto [hash, slot_idx] = HashAndSlot(key);
        Node** edge = &slots_[slot_idx];
        for (auto* node = *edge; node; node = node->next) {
            if (node->Equals(hash, key)) {
                *edge = node->next;
                node->Destroy();
                AddToFree(node);
                count_--;
                return true;
            }
            edge = &node->next;
        }
        return false;
    }

    /// Iterator for entries in the map.
    template <bool IS_CONST>
    class IteratorT {
      private:
        using MAP = std::conditional_t<IS_CONST, const HashmapBase, HashmapBase>;
        using NODE = std::conditional_t<IS_CONST, const Node, Node>;

      public:
        /// @returns the entry pointed to by this iterator
        auto& operator->() { return node_->Entry(); }

        /// @returns a reference to the entry at the iterator
        auto& operator*() { return node_->Entry(); }

        /// Increments the iterator
        /// @returns this iterator
        IteratorT& operator++() {
            node_ = node_->next;
            SkipEmptySlots();
            return *this;
        }

        /// Equality operator
        /// @param other the other iterator to compare this iterator to
        /// @returns true if this iterator is equal to other
        bool operator==(const IteratorT& other) const { return node_ == other.node_; }

        /// Inequality operator
        /// @param other the other iterator to compare this iterator to
        /// @returns true if this iterator is not equal to other
        bool operator!=(const IteratorT& other) const { return node_ != other.node_; }

      private:
        /// Friend class
        friend class HashmapBase;

        IteratorT(MAP& map, size_t slot, NODE* node) : map_(map), slot_(slot), node_(node) {
            SkipEmptySlots();
        }

        void SkipEmptySlots() {
            while (!node_ && slot_ + 1 < map_.slots_.Length()) {
                node_ = map_.slots_[++slot_];
            }
        }

        MAP& map_;
        size_t slot_ = 0;
        NODE* node_ = nullptr;
    };

    /// An immutable key and mutable value iterator
    using Iterator = IteratorT</*IS_CONST*/ false>;

    /// An immutable key and value iterator
    using ConstIterator = IteratorT</*IS_CONST*/ true>;

    /// @returns an immutable iterator to the start of the map.
    ConstIterator begin() const { return ConstIterator{*this, 0, slots_.Front()}; }

    /// @returns an immutable iterator to the end of the map.
    ConstIterator end() const { return ConstIterator{*this, slots_.Length(), nullptr}; }

    /// @returns an iterator to the start of the map.
    Iterator begin() { return Iterator{*this, 0, slots_.Front()}; }

    /// @returns an iterator to the end of the map.
    Iterator end() { return Iterator{*this, slots_.Length(), nullptr}; }

    /// STL-friendly alias to Entry. Used by gmock.
    using value_type = const Entry&;

  protected:
    struct Node {
        /// A structure that has the same size and alignment as Entry.
        /// Replacement for std::aligned_storage as this is broken on earlier versions of MSVC.
        struct alignas(alignof(ENTRY)) Storage {
            /// Byte array of length sizeof(ENTRY)
            uint8_t data[sizeof(ENTRY)];
        };

        void Destroy() { Entry().~ENTRY(); }

        /// @returns the storage reinterpreted as an `Entry&`
        ENTRY& Entry() { return *Bitcast<ENTRY*>(&storage.data[0]); }

        /// @returns the storage reinterpreted as a `const Entry&`
        const ENTRY& Entry() const { return *Bitcast<const ENTRY*>(&storage.data[0]); }

        const HashmapBase::Key& Key() const { return HashmapBase::Entry::KeyOf(Entry()); }

        template <typename T>
        bool Equals(size_t hash, T&& value) const {
            auto& key = Key();
            return key.hash == hash && HashmapBase::Equal{}(key.Value(), value);
        }

        Storage storage;
        Node* next = nullptr;
    };

    /// Result of Put()
    struct PutResult {
        /// Action taken by Put()
        enum MapAction {
            /// A new entry was added to the map
            kAdded,
            /// A existing entry in the map was replaced
            kReplaced,
            /// No action was taken as the map already contained an entry with the given key
            kKeptExisting,
        };

        /// A reference to the inserted entry slot.
        Entry& entry;

        /// Whether the insert replaced or added a new entry to the map.
        MapAction action = MapAction::kAdded;

        /// @returns true if the entry was added to the map, or an existing entry was replaced.
        operator bool() const { return action != MapAction::kKeptExisting; }
    };

    /// Copies the hashmap @p other into this empty hashmap.
    /// @note This hashmap must be empty before calling
    /// @param other the hashmap to copy
    void Copy(const HashmapBase& other) {
        Reserve(other.capacity_);
        slots_.Resize(other.slots_.Length());
        for (size_t slot_idx = 0; slot_idx < slots_.Length(); slot_idx++) {
            for (auto* o = other.slots_[slot_idx]; o; o = o->next) {
                Insert(slot_idx, o->Entry());
            }
        }
        count_ = other.count_;
    }

    /// Moves the the hashmap @p other into this empty hashmap.
    /// @note This hashmap must be empty before calling
    /// @param other the hashmap to move
    void Move(HashmapBase&& other) {
        Reserve(other.capacity_);
        slots_.Resize(other.slots_.Length());
        for (size_t slot_idx = 0; slot_idx < slots_.Length(); slot_idx++) {
            for (auto* o = other.slots_[slot_idx]; o; o = o->next) {
                Insert(slot_idx, std::move(o->Entry()));
            }
        }
        count_ = other.count_;
        other.Clear();
    }

    template <typename K, typename... V>
    PutResult Put(bool replace, K&& key, V&&... values) {
        GrowIfNeeded();

        const auto [hash, slot_idx] = HashAndSlot(key);
        if (auto* node = FindNode(hash, slot_idx, key)) {
            if (!replace) {
                return {node->Entry(), PutResult::kKeptExisting};
            }
            node->Entry() = Entry{Key{hash, std::forward<K>(key)}, std::forward<V>(values)...};
            return {node->Entry(), PutResult::kReplaced};
        }
        auto* node = Insert(slot_idx, Key{hash, std::forward<K>(key)}, std::forward<V>(values)...);
        return {node->Entry(), PutResult::kAdded};
    }

    template <typename... ARGS>
    Node* Insert(size_t slot_idx, ARGS&&... args) {
        auto* node = TakeFree();
        new (&node->Entry()) Entry{std::forward<ARGS>(args)...};
        AddToSlot(slot_idx, node);
        count_++;
        return node;
    }

    template <typename K>
    std::pair<size_t, size_t> HashAndSlot(K&& key) const {
        size_t hash = Hash{}(key);
        return {hash, hash % slots_.Length()};
    }

    void Rehash() {
        size_t num_slots = NumSlots(capacity_);
        decltype(slots_) old_slots;
        std::swap(slots_, old_slots);
        slots_.Resize(num_slots);
        for (size_t slot_idx = 0; slot_idx < old_slots.Length(); slot_idx++) {
            auto* node = old_slots[slot_idx];
            while (node) {
                auto next = node->next;
                AddToSlot(node->Key().hash % num_slots, node);
                node = next;
            }
        }
    }

    template <typename K>
    const Node* FindNode(size_t hash, size_t slot_idx, K&& key) const {
        for (auto* node = slots_[slot_idx]; node; node = node->next) {
            if (node->Equals(hash, key)) {
                return node;
            }
        }
        return nullptr;
    }

    template <typename K>
    Node* FindNode(size_t hash, size_t slot_idx, K&& key) {
        for (auto* node = slots_[slot_idx]; node; node = node->next) {
            if (node->Equals(hash, key)) {
                return node;
            }
        }
        return nullptr;
    }

    void AddToSlot(size_t slot_idx, Node* node) {
        node->next = slots_[slot_idx];
        slots_[slot_idx] = node;
    }

    void GrowIfNeeded() {
        if (!free_) {
            AllocateNodes(capacity_ * 2);
            Rehash();
        }
    }

    Node* TakeFree() {
        auto* node = free_;
        free_ = node->next;
        node->next = nullptr;
        return node;
    }

    void AddToFree(Node* node) {
        node->next = free_;
        free_ = node;
    }

    void AllocateNodes(size_t count) {
        auto* memory =
            reinterpret_cast<std::byte*>(malloc(sizeof(NodesAllocation) + sizeof(Node) * count));
        if (TINT_UNLIKELY(!memory)) {
            TINT_ICE() << "out of memory";
            return;
        }
        auto* nodes_allocation = Bitcast<NodesAllocation*>(memory);
        nodes_allocation->next = allocations_;
        allocations_ = nodes_allocation;

        auto* nodes = Bitcast<Node*>(memory + sizeof(NodesAllocation));
        for (size_t i = 0; i < count; i++) {
            AddToFree(&nodes[i]);
        }
        capacity_ += count;
    }

    struct NodesAllocation {
        NodesAllocation* next = nullptr;
        // Array of Node follow
    };

    Vector<Node*, NumSlots(N)> slots_;
    std::array<Node, kMinCapacity> fixed_;
    Node* free_ = nullptr;
    NodesAllocation* allocations_ = nullptr;
    size_t capacity_ = kMinCapacity;  // number of nodes (kMinCapacity + count_ + free)
    size_t count_ = 0;
};

}  // namespace tint

#endif  // SRC_TINT_UTILS_CONTAINERS_HASHMAP_BASE_H_
