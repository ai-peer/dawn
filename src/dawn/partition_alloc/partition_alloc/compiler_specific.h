// Copyright 2023 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_PARTITION_ALLOC_PARTITION_ALLOC_COMPILER_SPECIFIC_H_
#define SRC_DAWN_PARTITION_ALLOC_PARTITION_ALLOC_COMPILER_SPECIFIC_H_

// This file was copied from PartitionAlloc, and adapted to Dawn. This is used when PartitionAlloc
// optional dependency is missing.

// Compiler detection: Note: clang masquerades as GCC on POSIX and as MSVC on
// Windows. The best is to always follow this order:
//
// #if defined(__clang__)
// ...clang specifics
// #elif defined(__GNUC__)
// ...gcc specifics
// #elif defined(_MSC_VER)
// ...msvc specifics
// #else
// ...othersspecifics
// #endif

// A wrapper around `__has_attribute`, similar to HAS_CPP_ATTRIBUTE.
#if defined(__has_attribute)
#define PA_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define PA_HAS_ATTRIBUTE(x) 0
#endif

// Annotate a function indicating it should not be inlined.
// Use like:
//   NOINLINE void DoStuff() { ... }
#if defined(__clang__) && PA_HAS_ATTRIBUTE(noinline)
#define PA_NOINLINE [[clang::noinline]]
#elif defined(__GNUC__) && PA_HAS_ATTRIBUTE(noinline)
#define PA_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define PA_NOINLINE __declspec(noinline)
#else
#define PA_NOINLINE
#endif

#if defined(__clang__) && defined(NDEBUG) && PA_HAS_ATTRIBUTE(always_inline)
#define PA_ALWAYS_INLINE [[clang::always_inline]] inline
#elif defined(__GNUC__) && defined(NDEBUG) && PA_HAS_ATTRIBUTE(always_inline)
#define PA_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER) && defined(NDEBUG)
#define PA_ALWAYS_INLINE __forceinline
#else
#define PA_ALWAYS_INLINE inline
#endif

// Marks a type as being eligible for the "trivial" ABI despite having a
// non-trivial destructor or copy/move constructor. Such types can be relocated
// after construction by simply copying their memory, which makes them eligible
// to be passed in registers. The canonical example is std::unique_ptr.
//
// Use with caution; this has some subtle effects on constructor/destructor
// ordering and will be very incorrect if the type relies on its address
// remaining constant. When used as a function argument (by value), the value
// may be constructed in the caller's stack frame, passed in a register, and
// then used and destructed in the callee's stack frame. A similar thing can
// occur when values are returned.
//
// TRIVIAL_ABI is not needed for types which have a trivial destructor and
// copy/move constructors, such as base::TimeTicks and other POD.
//
// It is also not likely to be effective on types too large to be passed in one
// or two registers on typical target ABIs.
//
// See also:
//   https://clang.llvm.org/docs/AttributeReference.html#trivial-abi
//   https://libcxx.llvm.org/docs/DesignDocs/UniquePtrTrivialAbi.html
#if defined(__clang__) && PA_HAS_ATTRIBUTE(trivial_abi)
#define PA_TRIVIAL_ABI [[clang::trivial_abi]]
#else
#define PA_TRIVIAL_ABI
#endif

// Requires constant initialization. See constinit in C++20. Allows to rely on a
// variable being initialized before execution, and not requiring a global
// constructor.
#if PA_HAS_ATTRIBUTE(require_constant_initialization)
#define PA_CONSTINIT __attribute__((require_constant_initialization))
#else
#define PA_CONSTINIT
#endif

#if defined(__clang__)
#define PA_GSL_POINTER [[gsl::Pointer]]
#else
#define PA_GSL_POINTER
#endif

// Constexpr destructors were introduced in C++20. PartitionAlloc's minimum
// supported C++ version is C++17.
#if defined(__cpp_constexpr) && __cpp_constexpr >= 201907L
#define PA_CONSTEXPR_DTOR constexpr
#else
#define PA_CONSTEXPR_DTOR
#endif

// PA_ATTRIBUTE_RETURNS_NONNULL
//
// Tells the compiler that a function never returns a null pointer. Sourced from Abseil's
// `attributes.h`.
#if PA_HAS_ATTRIBUTE(returns_nonnull)
#define PA_ATTRIBUTE_RETURNS_NONNULL __attribute__((returns_nonnull))
#else
#define PA_ATTRIBUTE_RETURNS_NONNULL
#endif

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_COMPILER_SPECIFIC_H_
