// Copyright 2023 The Tint Authors.
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

#ifndef SRC_TINT_INTERP_MEMORY_H_
#define SRC_TINT_INTERP_MEMORY_H_

#include <cstdint>
#include <string>
#include <vector>

#include "tint/source.h"
#include "tint/type/address_space.h"
#include "tint/utils/block_allocator.h"

// Forward Declarations
namespace tint::constant {
class Value;
}  // namespace tint::constant
namespace tint::interp {
class Memory;
class ShaderExecutor;
}  // namespace tint::interp
namespace tint::type {
class Type;
}  // namespace tint::type

namespace tint::interp {

/// The type of an atomic read-modify-write operation.
enum class AtomicOp {
    kAdd,
    kSub,
    kMax,
    kMin,
    kAnd,
    kOr,
    kXor,
    kXchg,
};

class MemoryView {
  public:
    // Default constructor - produces an invalid memory view.
    MemoryView() : memory_(nullptr) {}

    /// @returns the address space of this memory view
    type::AddressSpace AddressSpace() const { return address_space_; }

    /// @returns the type of this memory view
    const type::Type* Type() const { return type_; }

    /// @returns the offset in bytes of this memory view from the start of the allocation
    uint64_t Offset() const { return offset_; }

    /// @returns the size in bytes of this memory view
    uint64_t Size() const { return size_; }

    /// @returns the parent memory view, or nullptr if this is a root memory view
    const MemoryView* Parent() const { return parent_; }

    /// @returns the source location that this memory view corresponds to
    const tint::Source& Source() const { return source_; }

    /// Load the value from this memory view.
    /// @returns the loaded value as a constant::Value
    const constant::Value* Load();

    /// Store a value to this memory view.
    /// @param value the value to store
    void Store(const constant::Value* value);

    /// Create a sub-view into the view.
    /// @param type the type of the sub-view
    /// @param offset the offset into this view that the new sub-view will start from
    /// @param size the size in bytes of the new sub-view
    /// @param source the source location that this sub-view is being created at
    /// @returns the sub-view
    MemoryView* CreateSubview(const type::Type* type,
                              uint64_t offset,
                              uint64_t size,
                              tint::Source source);

    /// Perform an atomic load on this memory view.
    /// @returns the loaded value
    const constant::Value* AtomicLoad();

    /// Perform an atomic store on this memory view.
    /// @param value the value to store
    void AtomicStore(const constant::Value* value);

    /// Perform an atomic read-modify-write operation on this memory view.
    /// @param op the operation to apply
    /// @param value the value to add
    /// @returns the original value
    const constant::Value* AtomicRMW(AtomicOp op, const constant::Value* value);

    /// Perform an atomic compare-exchange on this memory view.
    /// @param cmp the comparator value
    /// @param value the value to store
    /// @param exchanged receives `true` if an exchange was performed, otherwise `false`
    /// @returns the original value
    const constant::Value* AtomicCompareExchange(const constant::Value* cmp,
                                                 const constant::Value* value,
                                                 bool& exchanged);

  private:
    ShaderExecutor* executor_ = nullptr;
    Memory* memory_ = nullptr;
    MemoryView* parent_ = nullptr;
    type::AddressSpace address_space_;
    const type::Type* type_;
    uint64_t offset_;
    uint64_t size_;
    tint::Source source_;
    bool is_valid_;

    /// Constructor
    MemoryView(ShaderExecutor* executor,
               Memory* memory,
               MemoryView* parent,
               type::AddressSpace addrspace,
               const type::Type* type,
               uint64_t offset,
               uint64_t size,
               tint::Source source,
               bool valid);

    /// Load a value from this memory view at an offset.
    /// Used internally to load aggregate elements recursively.
    /// @param type the type of the value to load
    /// @param offset the offset at which to load it from
    /// @returns the loaded value as a constant::Value
    const constant::Value* Load(const type::Type* type, uint64_t offset);

    /// Store a value to this memory view at an offset.
    /// Used internally to store aggregate elements recursively.
    /// @param value the value to store
    /// @param offset the offset at which to store it
    void Store(const constant::Value* value, uint64_t offset);

    /// Report an out-of-bounds access.
    /// @param msg the error message
    void ReportOutOfBounds(std::string msg);

    // Allow the block allocator to construct MemoryView objects.
    friend utils::BlockAllocator<MemoryView>;
};

class Memory {
  public:
    /// Constructor
    explicit Memory(uint64_t size);

    /// Destructor
    ~Memory();

    /// @returns the size of this memory object in bytes
    uint64_t Size() const { return static_cast<uint64_t>(data_.size()); }

    /// @returns a raw pointer to the underlying data
    uint8_t* Data() { return data_.data(); }

    /// Creates a new memory view into this memory allocation.
    /// Raises an error if the view is not in bounds.
    /// @param executor the shader executor in which this view is going to be used
    /// @param addrspace the address space of this memory view
    /// @param type the type of the memory view
    /// @param offset the optional offset at which the view should start
    /// @param size the optional size of the view in bytes
    /// @param source the source location that this view is being created at
    /// @returns the new memory view
    MemoryView* CreateView(ShaderExecutor* executor,
                           type::AddressSpace addrspace,
                           const type::Type* type,
                           uint64_t offset,
                           uint64_t size,
                           Source source);

    /// Creates a new memory view into this memory allocation.
    /// The returned memory view will encompass the whole allocation.
    /// Raises an error if the view is not in bounds.
    /// @param executor the shader executor in which this view is going to be used
    /// @param addrspace the address space of this memory view
    /// @param type the type of the memory view
    /// @param source the source location that this view is being created at
    /// @returns the new memory view
    MemoryView* CreateView(ShaderExecutor* executor,
                           type::AddressSpace addrspace,
                           const type::Type* type,
                           Source source);

    /// Load `size` bytes at `offset` from this memory allocation, and store the result in `value`.
    void Load(void* value, uint64_t offset, uint64_t size);

    /// Store `size` bytes at `offset` to this memory allocation, from the contents of `value`.
    void Store(const void* value, uint64_t offset, uint64_t size);

    /// Copy data into this memory allocation from another one.
    void CopyFrom(uint64_t dst_offset, Memory& src, uint64_t src_offset, uint64_t size);

    /// Perform an atomic load from this memory allocation.
    template <typename T>
    T AtomicLoad(uint64_t offset);

    /// Perform an atomic store to this memory allocation.
    template <typename T>
    void AtomicStore(uint64_t offset, T value);

    /// Perform an atomic read-modify-write operation on this memory allocation.
    template <typename T>
    T AtomicRMW(uint64_t offset, AtomicOp op, T value);

    /// Perform an atomic compare-exchange operation on this memory allocation.
    template <typename T>
    T AtomicCompareExchange(uint64_t offset, T cmp, T value, bool& exchanged);

  private:
    std::vector<uint8_t> data_;
};

}  // namespace tint::interp

#endif  // SRC_TINT_INTERP_MEMORY_H_
