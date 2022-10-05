// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TINT_UTILS_BLOCK_ALLOCATOR_H_
#define SRC_TINT_UTILS_BLOCK_ALLOCATOR_H_

#include <cstring>
#include <utility>
#include <vector>

#include "src/tint/utils/bitcast.h"
#include "src/tint/utils/math.h"

namespace tint::utils {

/// A container and allocator of objects of (or deriving from) the template type `T`.
/// Objects are allocated by calling Create(), and are owned by the BlockAllocator.
/// When the BlockAllocator is destructed, all constructed objects are automatically destructed and
/// freed.
///
/// Objects held by the BlockAllocator can be iterated over using a View.
template <typename T, size_t BLOCK_SIZE = 64 * 1024, size_t BLOCK_ALIGNMENT = 16>
class BlockAllocator {
  public:
    /// An iterator type over the objects of the BlockAllocator
    using Iterator = T* const* const;

    /// An immutable iterator type over the objects of the BlockAllocator
    using ConstIterator = T const* const*;

    /// Alias to ConstIterator if IS_CONST is true, otherwise Iterator
    template <bool IS_CONST>
    using TIterator = std::conditional_t<IS_CONST, ConstIterator, Iterator>;

    /// View provides begin() and end() methods for looping over the objects owned by the
    /// BlockAllocator.
    template <bool IS_CONST>
    class TView {
      public:
        /// @returns an iterator to the beginning of the view
        TIterator<IS_CONST> begin() const {
            auto* root = &allocator_->data[0];
            if constexpr (IS_CONST) {
                return Bitcast<ConstIterator>(root);
            } else {
                return root;
            }
        }

        /// @returns an iterator to the end of the view
        TIterator<IS_CONST> end() const {
            auto* root = &allocator_->data[0];
            if constexpr (IS_CONST) {
                return Bitcast<ConstIterator>(root + allocator_->data.size());
            } else {
                return root + allocator_->data.size();
            }
        }

      private:
        friend BlockAllocator;  // For BlockAllocator::operator View()
        explicit TView(BlockAllocator const* allocator) : allocator_(allocator) {}
        BlockAllocator const* const allocator_;
    };

    /// View provides begin() and end() methods for looping over the objects owned by the
    /// BlockAllocator.
    using View = TView<false>;

    /// ConstView provides begin() and end() methods for looping over the objects owned by the
    /// BlockAllocator.
    using ConstView = TView<true>;

    /// Constructor
    BlockAllocator() = default;

    /// Move constructor
    /// @param rhs the BlockAllocator to move
    BlockAllocator(BlockAllocator&& rhs) { std::swap(data, rhs.data); }

    /// Move assignment operator
    /// @param rhs the BlockAllocator to move
    /// @return this BlockAllocator
    BlockAllocator& operator=(BlockAllocator&& rhs) {
        if (this != &rhs) {
            Reset();
            std::swap(data, rhs.data);
        }
        return *this;
    }

    /// Destructor
    ~BlockAllocator() { Reset(); }

    /// @return a View of all objects owned by this BlockAllocator
    View Objects() { return View(this); }

    /// @return a ConstView of all objects owned by this BlockAllocator
    ConstView Objects() const { return ConstView(this); }

    /// Creates a new `TYPE` owned by the BlockAllocator.
    /// When the BlockAllocator is destructed the object will be destructed and freed.
    /// @param args the arguments to pass to the type constructor
    /// @returns the pointer to the constructed object
    template <typename TYPE = T, typename... ARGS>
    TYPE* Create(ARGS&&... args) {
        static_assert(std::is_same<T, TYPE>::value || std::is_base_of<T, TYPE>::value,
                      "TYPE does not derive from T");
        static_assert(std::is_same<T, TYPE>::value || std::has_virtual_destructor<T>::value,
                      "TYPE requires a virtual destructor when calling Create() for a type "
                      "that is not T");

        auto* ptr = new TYPE(std::forward<ARGS>(args)...);
        data.push_back(ptr);

        return ptr;
    }

    /// Frees all allocations from the allocator.
    void Reset() {
        for (auto ptr : Objects()) {
            delete ptr;
        }
        data.clear();
    }

    /// @returns the total number of allocated objects.
    size_t Count() const { return data.size(); }

  private:
    BlockAllocator(const BlockAllocator&) = delete;
    BlockAllocator& operator=(const BlockAllocator&) = delete;

    std::vector<T*> data;
};

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_BLOCK_ALLOCATOR_H_
