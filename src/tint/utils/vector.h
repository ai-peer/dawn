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

#ifndef SRC_TINT_UTILS_VECTOR_H_
#define SRC_TINT_UTILS_VECTOR_H_

#include <stddef.h>
#include <utility>
#include <vector>

namespace tint::utils {

/// Forward declaration
template <typename T>
class VectorRef;

}  // namespace tint::utils

namespace tint::utils {

/// A slice represents a contigious array of elements of type T.
template <typename T>
struct Slice {
    /// The pointer to the first element in the slice
    T* data = nullptr;

    /// The total number of elements in the slice
    size_t len = 0;

    /// The total capacity of the backing store for the slice
    size_t cap = 0;

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    T& operator[](size_t i) { return data[i]; }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return data[i]; }
};

/// Vector is a dynamically-sized vector of contigious elements of type T.
/// Vector will fit `N` elements internally before spilling to heap allocations.
template <typename T, size_t N = 0>
class Vector {
  public:
    Vector() { Reset(); }

    /// Constructor
    /// @param length the initial length of the vector
    explicit Vector(size_t length) {
        Reset();
        Reserve(length);
        for (size_t i = 0; i < length; i++) {
            new (&slice_.data[i]) T{};
        }
        slice_.len = length;
    }

    /// Constructor
    /// @param elements the elements to place into the vector
    explicit Vector(std::initializer_list<T> elements) {
        Reset();
        Reserve(elements.size());
        for (auto& el : elements) {
            new (&slice_.data[slice_.len++]) T{el};
        }
    }

    /// Copy constructor
    /// @param other the vector to copy
    Vector(const Vector& other) {
        Reset();
        Copy(other.slice_);
    }

    /// Move constructor
    /// @param other the vector to move
    Vector(Vector&& other) {
        Reset();
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Copy constructor (differing N length)
    /// @param other the vector to copy
    template <size_t N2>
    Vector(const Vector<T, N2>& other) {
        Reset();
        Copy(other.slice_);
    }

    /// Move constructor (differing N length)
    /// @param other the vector to move
    template <size_t N2>
    Vector(Vector<T, N2>&& other) {
        Reset();
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Move constructor from a vector reference
    /// @param other the vector reference to move
    Vector(VectorRef<T>&& other) {  // NOLINT
        Reset();
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Destructor
    ~Vector() {
        Clear();
        FreeData(slice_.data);
    }

    /// Assignment operator
    /// @param other the vector to copy
    /// @returns this vector so calls can be chained
    Vector& operator=(const Vector<T>& other) {
        Copy(other.slice_);
        return *this;
    }

    /// Move operator
    /// @param other the vector to move
    /// @returns this vector so calls can be chained
    Vector& operator=(Vector<T>&& other) {
        MoveOrCopy(VectorRef<T>(std::move(other)));
        return *this;
    }

    /// Assignment operator (differing N length)
    /// @param other the vector to copy
    /// @returns this vector so calls can be chained
    template <size_t N2>
    Vector& operator=(const Vector<T, N2>& other) {
        Copy(other.slice_);
        return *this;
    }

    /// Move operator (differing N length)
    /// @param other the vector to copy
    /// @returns this vector so calls can be chained
    template <size_t N2>
    Vector& operator=(Vector<T, N2>&& other) {
        MoveOrCopy(VectorRef<T>(std::move(other)));
        return *this;
    }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    T& operator[](size_t i) { return slice_[i]; }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return slice_[i]; }

    /// @return the number of elements in the vector
    size_t Length() const { return slice_.len; }

    /// @return the number of elements that the vector could hold before a heap allocation needs to
    /// be made
    size_t Capacity() const { return slice_.cap; }

    /// Reserves memory to hold at least `new_cap` elements
    /// @param new_cap the new vector capacity
    void Reserve(size_t new_cap) {
        if (new_cap > slice_.cap) {
            auto* old_data = slice_.data;
            AllocateData(new_cap);
            for (size_t i = 0; i < slice_.len; i++) {
                new (&slice_.data[i]) T(std::move(old_data[i]));
                old_data[i].~T();
            }
            FreeData(old_data);
        }
    }

    /// Copies all the elements from `other` to this vector, replacing the content of this vector.
    /// @param other the
    template <typename T2, size_t N2>
    void Copy(const Vector<T2, N2>& other) {
        Copy(other.slice_);
    }

    /// Clears all elements from the vector, keeping the capacity the same.
    void Clear() {
        for (size_t i = 0; i < slice_.len; i++) {
            slice_.data[i].~T();
        }
        slice_.len = 0;
    }

    /// @returns true if the vector is empty.
    bool IsEmpty() const { return slice_.len == 0; }

    /// Appends a new element to the vector.
    /// @param el the element to append.
    void Push(const T& el) {
        if (slice_.len >= slice_.cap) {
            Grow();
        }
        new (&slice_.data[slice_.len++]) T(el);
    }

    /// Appends a new element to the vector.
    /// @param el the element to append.
    void Push(T&& el) {
        if (slice_.len >= slice_.cap) {
            Grow();
        }
        new (&slice_.data[slice_.len++]) T(std::forward<T>(el));
    }

    /// Removes and returns the last element from the vector.
    /// @returns the popped element
    T Pop() { return std::move(slice_.data[--slice_.len]); }

    /// @returns a reference to the first element in the vector
    T& Front() const { return slice_.data[0]; }

    /// @returns a reference to the last element in the vector
    T& Back() const { return slice_.data[slice_.len - 1]; }

    /// @returns a pointer to the first element in the vector
    T* begin() const { return slice_.data; }

    /// @returns a pointer to one past the last element in the vector
    T* end() const { return slice_.data + slice_.len; }

  private:
    /// Friend classes
    template <typename, size_t>
    friend class Vector;

    template <typename>
    friend class VectorRef;

    /// Resets the slice_ field to the initial values on construction
    void Reset() {
        if constexpr (HasSmallArray) {
            slice_.data = &small_[0];
        } else {
            slice_.data = nullptr;
        }
        slice_.len = 0;
        slice_.cap = N;
    }

    /// @returns true if slice_.data points to the first element of small_
    bool IsHeapAllocation() const {
        if constexpr (HasSmallArray) {
            return slice_.data != &small_[0];
        } else {
            return true;
        }
    }

    /// Expands the capacity of the vector
    void Grow() { Reserve(slice_.cap * 2); }

    /// Moves 'other' to this vector, if possible, otherwise performs a copy.
    void MoveOrCopy(VectorRef<T>&& other) {
        if (other.slice == &slice_) {
            return;  // other is this.
        }
        if (other.can_move) {
            slice_ = *other.slice;
            *other.slice = {};
        } else {
            Copy(*other.slice);
        }
    }

    /// Copies all the elements from `other` to this vector, replacing the content of this vector.
    /// @param other the
    void Copy(const Slice<T>& other) {
        Clear();

        if (slice_.cap < other.len) {
            FreeData(slice_.data);
            AllocateData(other.len);
        }

        slice_.len = other.len;
        for (size_t i = 0; i < slice_.len; i++) {
            new (&slice_.data[i]) T{other.data[i]};
        }
    }

    /// Heap-allocates a new vector of T, assigning it to data, and updates cap and move_policy.
    void AllocateData(size_t new_cap) {
        if constexpr (HasSmallArray) {
            if (new_cap < N) {
                slice_.data = &small_[0];
                slice_.cap = N;
                return;
            }
        }
        slice_.data = new T[new_cap];
        slice_.cap = new_cap;
    }

    /// Heap-frees data, if isn't a pointer to small_
    void FreeData(T* data) {
        if (!data) {
            return;
        }
        if constexpr (HasSmallArray) {
            if (data == &small_[0]) {
                return;
            }
        }
        delete[] data;
        data = nullptr;
    }

    constexpr static bool HasSmallArray = N > 0;
    struct NoSmallArray {};
    using SmallArray = std::conditional_t<HasSmallArray, T[HasSmallArray ? N : 1], NoSmallArray>;

    Slice<T> slice_;
    SmallArray small_;
};

/// VectorRef is a r-value reference to a Vector, used to pass vectors as parameters, avoiding
/// copies between the caller and the callee. VectorRef can accept a Vector of any 'N' width
/// decoupling the caller's vector internal size from the callee's vector size. VectorRef only
/// supports moves, not copies.
template <typename T>
class VectorRef {
  public:
    /// Constructor from a Vector r-value
    /// @param vector the vector being moved
    template <size_t N>
    VectorRef(Vector<T, N>&& vector)  // NOLINT
        : slice(&vector.slice_), can_move(vector.IsHeapAllocation()) {}

    /// Move constructor
    /// @param other the vector reference to move
    VectorRef(VectorRef&& other) = default;

    /// Deleted copy constructor
    VectorRef(const VectorRef& other) = delete;

    /// The slice of the vector being referenced.
    Slice<T>* slice = nullptr;
    /// Whether the slice data is passed by r-value reference, and can be moved.
    bool can_move = false;
};

/// Helper for converting a Vector to a std::vector.
/// @note This helper exists to help code migration. Avoid if possible.
template <typename T, size_t N>
std::vector<T> ToVector(const Vector<T, N>& vector) {
    std::vector<T> out;
    out.reserve(vector.Length());
    for (auto& el : vector) {
        out.emplace_back(el);
    }
    return out;
}

/// Helper for converting a std::vector to a Vector.
/// @note This helper exists to help code migration. Avoid if possible.
template <typename T, size_t N = 0>
Vector<T, N> ToVector(const std::vector<T>& vector) {
    Vector<T, N> out;
    out.Reserve(vector.size());
    for (auto& el : vector) {
        out.Push(el);
    }
    return out;
}

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_VECTOR_H_
