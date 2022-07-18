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
#include <stdint.h>
#include <array>
#include <utility>
#include <vector>

#include "src/tint/utils/bitcast.h"

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

    /// @returns a reference to the first element in the vector
    T& Front() const { return data[0]; }

    /// @returns a reference to the last element in the vector
    T& Back() const { return data[len - 1]; }

    /// @returns a pointer to the first element in the vector
    T* begin() const { return data; }

    /// @returns a pointer to one past the last element in the vector
    T* end() const { return data + len; }
};

/// Vector is a dynamically-sized vector of contigious elements of type T.
/// Vector will fit `N` elements internally before spilling to heap allocations.
template <typename T, size_t N = 0>
class Vector {
  public:
    Vector() { impl_.Reset(); }

    /// Constructor
    /// @param length the initial length of the vector
    explicit Vector(size_t length) {
        impl_.Reset();
        Reserve(length);
        for (size_t i = 0; i < length; i++) {
            new (&impl_.slice.data[i]) T{};
        }
        impl_.slice.len = length;
    }

    /// Constructor
    /// @param length the initial length of the vector
    /// @param value the value to copy into each element of the vector
    explicit Vector(size_t length, T value) {
        impl_.Reset();
        Reserve(length);
        for (size_t i = 0; i < length; i++) {
            new (&impl_.slice.data[i]) T{value};
        }
        impl_.slice.len = length;
    }

    /// Constructor
    /// @param elements the elements to place into the vector
    explicit Vector(std::initializer_list<T> elements) {
        impl_.Reset();
        Reserve(elements.size());
        for (auto& el : elements) {
            new (&impl_.slice.data[impl_.slice.len++]) T{el};
        }
    }

    /// Copy constructor
    /// @param other the vector to copy
    Vector(const Vector& other) {
        impl_.Reset();
        Copy(other.impl_.slice);
    }

    /// Move constructor
    /// @param other the vector to move
    Vector(Vector&& other) {
        impl_.Reset();
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Copy constructor (differing N length)
    /// @param other the vector to copy
    template <size_t N2>
    Vector(const Vector<T, N2>& other) {
        impl_.Reset();
        Copy(other.impl_.slice);
    }

    /// Move constructor (differing N length)
    /// @param other the vector to move
    template <size_t N2>
    Vector(Vector<T, N2>&& other) {
        impl_.Reset();
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Move constructor from a vector reference
    /// @param other the vector reference to move
    Vector(VectorRef<T>&& other) {  // NOLINT
        impl_.Reset();
        MoveOrCopy(VectorRef<T>(std::move(other)));
    }

    /// Destructor
    ~Vector() {
        Clear();
        impl_.Free(impl_.slice.data);
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
        Copy(other.impl_.slice);
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
    T& operator[](size_t i) { return impl_.slice[i]; }

    /// Index operator
    /// @param i the element index. Must be less than `len`.
    /// @returns a reference to the i'th element.
    const T& operator[](size_t i) const { return impl_.slice[i]; }

    /// @return the number of elements in the vector
    size_t Length() const { return impl_.slice.len; }

    /// @return the number of elements that the vector could hold before a heap allocation needs to
    /// be made
    size_t Capacity() const { return impl_.slice.cap; }

    /// Reserves memory to hold at least `new_cap` elements
    /// @param new_cap the new vector capacity
    void Reserve(size_t new_cap) {
        if (new_cap > impl_.slice.cap) {
            auto* old_data = impl_.slice.data;
            impl_.Allocate(new_cap);
            for (size_t i = 0; i < impl_.slice.len; i++) {
                new (&impl_.slice.data[i]) T(std::move(old_data[i]));
                old_data[i].~T();
            }
            impl_.Free(old_data);
        }
    }

    /// Resizes the vector to the given length, expanding capacity if necessary.
    /// New elements are zero-initialized
    /// @param new_len the new vector length
    void Resize(size_t new_len) {
        Reserve(new_len);
        for (size_t i = impl_.slice.len; i > new_len; i--) {
            impl_.slice.data[i].~T();  // Shrink
        }
        for (size_t i = impl_.slice.len; i < new_len; i++) {
            new (&impl_.slice.data[i]) T{};  // Grow
        }
    }

    /// Copies all the elements from `other` to this vector, replacing the content of this vector.
    /// @param other the
    template <typename T2, size_t N2>
    void Copy(const Vector<T2, N2>& other) {
        Copy(other.impl_.slice);
    }

    /// Clears all elements from the vector, keeping the capacity the same.
    void Clear() {
        for (size_t i = 0; i < impl_.slice.len; i++) {
            impl_.slice.data[i].~T();
        }
        impl_.slice.len = 0;
    }

    /// Appends a new element to the vector.
    /// @param el the element to append.
    void Push(const T& el) {
        if (impl_.slice.len >= impl_.slice.cap) {
            Grow();
        }
        new (&impl_.slice.data[impl_.slice.len++]) T(el);
    }

    /// Appends a new element to the vector.
    /// @param el the element to append.
    void Push(T&& el) {
        if (impl_.slice.len >= impl_.slice.cap) {
            Grow();
        }
        new (&impl_.slice.data[impl_.slice.len++]) T(std::forward<T>(el));
    }

    /// Removes and returns the last element from the vector.
    /// @returns the popped element
    T Pop() { return std::move(impl_.slice.data[--impl_.slice.len]); }

    /// @returns true if the vector is empty.
    bool IsEmpty() const { return impl_.slice.len == 0; }

    /// @returns a reference to the first element in the vector
    T& Front() const { return impl_.slice.Front(); }

    /// @returns a reference to the last element in the vector
    T& Back() const { return impl_.slice.Back(); }

    /// @returns a pointer to the first element in the vector
    T* begin() const { return impl_.slice.begin(); }

    /// @returns a pointer to one past the last element in the vector
    T* end() const { return impl_.slice.end(); }

  private:
    /// Friend class (differing specializations of this class)
    template <typename, size_t>
    friend class Vector;

    /// Friend class
    template <typename>
    friend class VectorRef;

    /// Expands the capacity of the vector
    void Grow() { Reserve(impl_.slice.cap * 2); }

    /// Moves 'other' to this vector, if possible, otherwise performs a copy.
    void MoveOrCopy(VectorRef<T>&& other) {
        if (&other.slice_ == &impl_.slice) {
            return;  // other is this.
        }
        if (other.can_move_) {
            impl_.slice = other.slice_;
            other.slice_ = {};
        } else {
            Copy(other.slice_);
        }
    }

    /// Copies all the elements from `other` to this vector, replacing the content of this vector.
    /// @param other the
    void Copy(const Slice<T>& other) {
        Clear();

        if (impl_.slice.cap < other.len) {
            impl_.Free(impl_.slice.data);
            impl_.Allocate(other.len);
        }

        impl_.slice.len = other.len;
        for (size_t i = 0; i < impl_.slice.len; i++) {
            new (&impl_.slice.data[i]) T{other.data[i]};
        }
    }

    constexpr static bool HasSmallArray = N > 0;

    /// Replacement for std::aligned_storage as this is broken on earlier versions of MSVC.
    struct alignas(alignof(T)) TStorage {
        /// @returns the storage reinterpreted as a T*
        T* Get() { return Bitcast<T*>(&data[0]); }
        /// @returns the storage reinterpreted as a T*
        const T* Get() const { return Bitcast<const T*>(&data[0]); }
        /// Byte array of length sizeof(T)
        uint8_t data[sizeof(T)];
    };

    struct ImplWithSmallArray {
        Slice<T> slice;

        std::array<TStorage, N> small_arr;

        /// Resets the slice field to the initial values on construction
        void Reset() {
            slice.data = small_arr[0].Get();
            slice.len = 0;
            slice.cap = N;
        }

        /// Allocates a new vector of T either from small_arr, or from the heap, then
        /// assigns the pointer it to slice.data, and updates cap and move_policy.
        void Allocate(size_t new_cap) {
            if constexpr (HasSmallArray) {
                if (new_cap < N) {
                    slice.data = small_arr[0].Get();
                    slice.cap = N;
                    return;
                }
            }
            slice.data = Bitcast<T*>(new TStorage[new_cap]);
            slice.cap = new_cap;
        }

        /// Heap-frees data, if isn't a pointer to small_arr
        void Free(T* data) const {
            if (data && data != small_arr[0].Get()) {
                delete[] Bitcast<TStorage*>(data);
            }
        }

        /// @returns true if slice.data does not point to small_arr
        bool IsHeapAllocation() const { return slice.data != small_arr[0].Get(); }
    };

    struct ImplWithoutSmallArray {
        Slice<T> slice;

        /// Resets the slice field to the initial values on construction
        void Reset() {
            slice.data = nullptr;
            slice.len = 0;
            slice.cap = N;
        }

        /// Heap-allocates a new vector of T, assigning it to slice.data, and updates cap and
        /// move_policy.
        void Allocate(size_t new_cap) {
            slice.data = reinterpret_cast<T*>(new TStorage[new_cap]);
            slice.cap = new_cap;
        }

        /// Frees data.
        void Free(T* data) const {
            if (data) {
                delete[] reinterpret_cast<TStorage*>(data);
            }
        }

        /// @returns true
        bool IsHeapAllocation() const { return true; }
    };

    using Impl = std::conditional_t<HasSmallArray, ImplWithSmallArray, ImplWithoutSmallArray>;
    Impl impl_;
};

/// VectorRef is a r-value reference to a Vector, used to pass vectors as parameters, avoiding
/// copies between the caller and the callee. VectorRef can accept a Vector of any 'N' width
/// decoupling the caller's vector internal size from the callee's vector size. VectorRef only
/// supports moves, not copies.
template <typename T>
class VectorRef {
  public:
    /// Copy constructor
    /// @param other the vector reference
    VectorRef(const VectorRef& other) : slice_(other.slice_), can_move_(false) {}

    /// Move constructor
    /// @param other the vector reference
    VectorRef(VectorRef&& other) = default;

    /// Constructor from a Vector.
    /// @param vector the vector reference
    template <size_t N>
    VectorRef(const Vector<T, N>& vector)  // NOLINT
        : slice_(const_cast<Slice<T>&>(vector.impl_.slice)), can_move_(false) {}

    /// Constructor from a std::move()'d Vector
    /// @param vector the vector being moved
    template <size_t N>
    VectorRef(Vector<T, N>&& vector)  // NOLINT
        : slice_(vector.impl_.slice), can_move_(vector.impl_.IsHeapAllocation()) {}

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

    /// @returns true if the vector is empty.
    bool IsEmpty() const { return slice_.len == 0; }

    /// @returns a reference to the first element in the vector
    T& Front() const { return slice_.Front(); }

    /// @returns a reference to the last element in the vector
    T& Back() const { return slice_.Back(); }

    /// @returns a pointer to the first element in the vector
    T* begin() const { return slice_.begin(); }

    /// @returns a pointer to one past the last element in the vector
    T* end() const { return slice_.end(); }

  private:
    /// Friend classes
    template <typename, size_t>
    friend class Vector;

    /// The slice of the vector being referenced.
    Slice<T>& slice_ = nullptr;
    /// Whether the slice data is passed by r-value reference, and can be moved.
    bool can_move_ = false;
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

template <typename T, typename... Ts>
auto MakeVector(Ts&&... elements) {
    return Vector<T, sizeof...(Ts)>({std::forward<Ts>(elements)...});
}

}  // namespace tint::utils

#endif  // SRC_TINT_UTILS_VECTOR_H_
