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

#ifndef SRC_DAWN_NATIVE_CHAINUTILS_H_
#define SRC_DAWN_NATIVE_CHAINUTILS_H_

#include <bitset>
#include <string>
#include <tuple>
#include <type_traits>

#include "absl/strings/str_format.h"
#include "dawn/common/Math.h"
#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/Error.h"

namespace dawn::native {

// Unpacks chained structures in a best effort manner (skipping unknown chains) without applying
// validation. If the same structure is duplicated in the chain, it is unspecified which one the
// result of Get will be. These are the effective constructors for the wrapper types. Note that
// these are implemented in the generated ChainUtils_autogen.cpp file.
template <typename T,
          typename Enable =
              std::enable_if_t<detail::ExtensibilityFor<T>::value == detail::Extensibility::In>>
Unpacked<T> Unpack(const T* chain);
template <typename T,
          typename Enable =
              std::enable_if_t<detail::ExtensibilityFor<T>::value == detail::Extensibility::Out>>
UnpackedOut<T> UnpackOut(T* chain);

// Unpacks chained structures into Unpacked<T> or UnpackedOut<T> types while applying validation.
template <typename T,
          typename Enable =
              std::enable_if_t<detail::ExtensibilityFor<T>::value == detail::Extensibility::In>>
ResultOrError<Unpacked<T>> ValidateAndUnpack(const T* chain);
template <typename T,
          typename Enable =
              std::enable_if_t<detail::ExtensibilityFor<T>::value == detail::Extensibility::Out>>
ResultOrError<UnpackedOut<T>> ValidateAndUnpackOut(T* chain);

//
// Wrapper classes for unpacked pointers. The classes essentially acts like a const T* or T* with
// the additional capabilities to validate and retrieve chained structures.
//

template <typename T>
class Unpacked {
  public:
    using TupleType = typename detail::UnpackedTypeFor<T>::Type;
    using BitsetType = typename std::bitset<std::tuple_size_v<TupleType>>;

    Unpacked() : mStruct(nullptr) {}

    operator bool() const { return mStruct != nullptr; }
    const T* operator->() const { return mStruct; }
    const T* operator*() const { return mStruct; }

    // Returns true iff every allowed chain in this unpacked type is nullptr.
    bool Empty() const;
    // Returns a string of the non-nullptr STypes from an unpacked chain.
    std::string ToString() const;

    template <typename In>
    auto Get() const {
        return std::get<std::add_const_t<In>*>(mUnpacked);
    }

    // Validation functions. See implementations of these below for usage, details, and examples.
    template <typename... Branches>
    ResultOrError<wgpu::SType> ValidateBranches() const;
    template <typename... Allowed>
    MaybeError ValidateSubset() const;

  private:
    friend Unpacked<T> Unpack<T>(const T* chain);
    friend ResultOrError<Unpacked<T>> ValidateAndUnpack<T>(const T* chain);

    explicit Unpacked(const T* packed) : mStruct(packed) {}

    const T* mStruct = nullptr;
    TupleType mUnpacked;
    BitsetType mBitset;
};

template <typename T>
class UnpackedOut {
  public:
    using TupleType = typename detail::UnpackedTypeFor<T>::Type;
    using BitsetType = typename std::bitset<std::tuple_size_v<TupleType>>;

    UnpackedOut() : mStruct(nullptr) {}

    operator bool() const { return mStruct != nullptr; }
    T* operator->() const { return mStruct; }
    T* operator*() const { return mStruct; }

    // Returns true iff every allowed chain in this unpacked type is nullptr.
    bool Empty() const;
    // Returns a string of the non-nullptr STypes from an unpacked chain.
    std::string ToString() const;

    template <typename Out>
    auto Get() const {
        return std::get<Out*>(mUnpacked);
    }

    // Validation functions. See implementations of these below for usage, details, and examples.
    template <typename... Branches>
    ResultOrError<wgpu::SType> ValidateBranches() const;
    template <typename... Allowed>
    MaybeError ValidateSubset() const;

  private:
    friend UnpackedOut<T> UnpackOut<T>(T* chain);
    friend ResultOrError<UnpackedOut<T>> ValidateAndUnpackOut<T>(T* chain);

    explicit UnpackedOut(T* packed) : mStruct(packed) {}

    T* mStruct = nullptr;
    TupleType mUnpacked;
    BitsetType mBitset;
};

// Tuple type of a Branch and an optional list of corresponding Extensions.
template <typename B, typename... Exts>
struct Branch;

// ------------------------------------------------------------------------------------------------
// Implementation details start here so that the headers are terse.
// ------------------------------------------------------------------------------------------------

namespace detail {

// Helpers to get the index in an unpacked tuple type for a particular type.
template <typename UnpackedT, typename Ext>
struct UnpackedTupleIndexOf;
template <typename Ext, typename... Exts>
struct UnpackedTupleIndexOf<std::tuple<Ext, Exts...>, Ext> {
    static constexpr size_t value = 0;
};
template <typename Ext, typename Other, typename... Exts>
struct UnpackedTupleIndexOf<std::tuple<Other, Exts...>, Ext> {
    static constexpr size_t value = 1 + UnpackedTupleIndexOf<std::tuple<Exts...>, Ext>::value;
};

template <typename UnpackedT, typename Ext>
struct UnpackedIndexOf;
template <typename T, typename Ext>
struct UnpackedIndexOf<Unpacked<T>, Ext> {
    static constexpr size_t value =
        UnpackedTupleIndexOf<typename Unpacked<T>::TupleType, const Ext*>::value;
};
template <typename T, typename Ext>
struct UnpackedIndexOf<Unpacked<T>, const Ext*> {
    static constexpr size_t value =
        UnpackedTupleIndexOf<typename Unpacked<T>::TupleType, const Ext*>::value;
};
template <typename T, typename Ext>
struct UnpackedIndexOf<UnpackedOut<T>, Ext> {
    static constexpr size_t value =
        UnpackedTupleIndexOf<typename UnpackedOut<T>::TupleType, Ext*>::value;
};
template <typename T, typename Ext>
struct UnpackedIndexOf<UnpackedOut<T>, Ext*> {
    static constexpr size_t value =
        UnpackedTupleIndexOf<typename UnpackedOut<T>::TupleType, Ext*>::value;
};

template <typename UnpackedT, typename... Exts>
struct UnpackedBitsetForExts {
    // Currently using an internal 64-bit unsigned int for internal representation. This is
    // necessary because std::bitset::operator| is not constexpr until C++23.
    static constexpr auto value = typename UnpackedT::BitsetType(
        ((uint64_t(1) << UnpackedIndexOf<UnpackedT, Exts>::value) | ...));
};

template <typename UnpackedT, typename...>
struct OneBranchValidator;
template <typename UnpackedT, typename B, typename... Exts>
struct OneBranchValidator<UnpackedT, Branch<B, Exts...>> {
    using BitsetType = typename UnpackedT::BitsetType;

    static bool Validate(const UnpackedT& unpacked, const BitsetType& actual, wgpu::SType& match) {
        // Only check the full bitset when the main branch matches.
        if (unpacked.template Get<B>() != nullptr) {
            // Allowed set of extensions includes the branch root as well.
            constexpr auto allowed = UnpackedBitsetForExts<UnpackedT, B, Exts...>::value;

            // The configuration is allowed if the actual available chains are a subset.
            if (IsSubset(actual, allowed)) {
                match = STypeFor<B>;
                return true;
            }
        }
        return false;
    }

    static std::string ToString() {
        if constexpr (sizeof...(Exts) > 0) {
            return absl::StrFormat("[ %s -> (%s) ]", STypesToString<B>(),
                                   STypesToString<Exts...>());
        } else {
            return absl::StrFormat("[ %s ]", STypesToString<B>());
        }
    }
};

template <typename UnpackedT, typename... Branches>
struct BranchesValidator {
    using BitsetType = typename UnpackedT::BitsetType;

    static bool Validate(const UnpackedT& unpacked, const BitsetType& actual, wgpu::SType& match) {
        return ((OneBranchValidator<UnpackedT, Branches>::Validate(unpacked, actual, match)) ||
                ...);
    }

    static std::string ToString() {
        return ((absl::StrFormat("  - %s\n", OneBranchValidator<UnpackedT, Branches>::ToString())) +
                ...);
    }
};

template <typename UnpackedT, typename...>
struct SubsetValidator;
template <typename T, typename... Allowed>
struct SubsetValidator<Unpacked<T>, Allowed...> {
    using BitsetType = typename Unpacked<T>::BitsetType;

    static std::string ToString() {
        return absl::StrFormat("[ %s ]", STypesToString<Allowed...>());
    }

    static MaybeError Validate(const Unpacked<T>& unpacked, const BitsetType& bitset) {
        // Allowed set of extensions includes the branch root as well.
        constexpr auto allowed =
            detail::UnpackedBitsetForExts<Unpacked<T>, std::add_const_t<Allowed>*...>::value;
        if (!IsSubset(bitset, allowed)) {
            return DAWN_VALIDATION_ERROR(
                "Expected extension set to be a subset of:\n%sInstead found: %s", ToString(),
                unpacked.ToString());
        }
        return {};
    }
};
template <typename T, typename... Allowed>
struct SubsetValidator<UnpackedOut<T>, Allowed...> {
    using BitsetType = typename UnpackedOut<T>::BitsetType;

    static std::string ToString() {
        return absl::StrFormat("[ %s ]", STypesToString<Allowed...>());
    }

    static MaybeError Validate(const UnpackedOut<T>& unpacked, const BitsetType& bitset) {
        // Allowed set of extensions includes the branch root as well.
        constexpr auto allowed = detail::UnpackedBitsetForExts<UnpackedOut<T>, Allowed*...>::value;
        if (!IsSubset(bitset, allowed)) {
            return DAWN_VALIDATION_ERROR(
                "Expected extension set to be a subset of:\n%sInstead found: %s", ToString(),
                unpacked.ToString());
        }
        return {};
    }
};

}  // namespace detail

template <typename T>
bool Unpacked<T>::Empty() const {
    bool result = true;
    std::apply(
        [&](const auto*... args) {
            (([&](const auto* arg) {
                 if (arg != nullptr) {
                     DAWN_ASSERT(
                         mBitset.test(detail::UnpackedIndexOf<Unpacked<T>, decltype(arg)>::value));
                     result = false;
                 }
             }(args)),
             ...);
        },
        mUnpacked);
    return result;
}
template <typename T>
bool UnpackedOut<T>::Empty() const {
    bool result = true;
    std::apply(
        [&](auto*... args) {
            (([&](auto* arg) {
                 if (arg != nullptr) {
                     DAWN_ASSERT(mBitset.test(
                         detail::UnpackedIndexOf<UnpackedOut<T>, decltype(arg)>::value));
                     result = false;
                 }
             }(args)),
             ...);
        },
        mUnpacked);
    return result;
}

template <typename T>
std::string Unpacked<T>::ToString() const {
    std::string result = "( ";
    std::apply(
        [&](const auto*... args) {
            (([&](const auto* arg) {
                 if (arg != nullptr) {
                     // reinterpret_cast because this chained struct might be forward-declared
                     // without a definition. The definition may only be available on a
                     // particular backend.
                     const auto* chainedStruct = reinterpret_cast<const wgpu::ChainedStruct*>(arg);
                     result += absl::StrFormat("%s, ", chainedStruct->sType);
                 }
             }(args)),
             ...);
        },
        mUnpacked);
    result += " )";
    return result;
}
template <typename T>
std::string UnpackedOut<T>::ToString() const {
    std::string result = "( ";
    std::apply(
        [&](auto*... args) {
            (([&](auto* arg) {
                 if (arg != nullptr) {
                     // reinterpret_cast because this chained struct might be forward-declared
                     // without a definition. The definition may only be available on a
                     // particular backend.
                     const auto* chainedStruct = reinterpret_cast<wgpu::ChainedStructOut*>(arg);
                     result += absl::StrFormat("%s, ", chainedStruct->sType);
                 }
             }(args)),
             ...);
        },
        mUnpacked);
    result += " )";
    return result;
}

// Validates that an unpacked chain retrieved via ValidateAndUnpack matches a valid "branch",
// where a "branch" is defined as a required "root" extension and optional follow-up
// extensions.
//
// Returns the wgpu::SType associated with the "root" extension of a "branch" if matched,
// otherwise returns an error.
//
// Example usage:
//     Unpacked<T> u;
//     DAWN_TRY_ASSIGN(u, ValidateAndUnpack(desc));
//     wgpu::SType rootType;
//     DAWN_TRY_ASSIGN(rootType,
//         u.ValidateBranches<Branch<Root1>, Branch<Root2, R2Ext1>>());
//     switch (rootType) {
//         case Root1: {
//             <do something>
//         }
//         case Root2: {
//             R2Ext1 ext = u.Get<R2Ext1>(u);
//             if (ext) {
//                 <do something with optional extension(s)>
//             }
//         }
//         default:
//             DAWN_UNREACHABLE();
//     }
//
// The example above checks that the unpacked chain is either:
//     - only a Root1 extension
//     - or a Root2 extension with an optional R2Ext1 extension
// Any other configuration is deemed invalid.
template <typename T>
template <typename... Branches>
ResultOrError<wgpu::SType> Unpacked<T>::ValidateBranches() const {
    using Validator = detail::BranchesValidator<Unpacked<T>, Branches...>;

    wgpu::SType match = wgpu::SType::Invalid;
    if (Validator::Validate(*this, mBitset, match)) {
        return match;
    }
    return DAWN_VALIDATION_ERROR(
        "Expected chain root to match one of the following branch types with optional extensions:"
        "\n%sInstead found: %s",
        Validator::ToString(), ToString());
}
template <typename T>
template <typename... Branches>
ResultOrError<wgpu::SType> UnpackedOut<T>::ValidateBranches() const {
    using Validator = detail::BranchesValidator<UnpackedOut<T>, Branches...>;

    wgpu::SType match = wgpu::SType::Invalid;
    if (Validator::Validate(*this, mBitset, match)) {
        return match;
    }
    return DAWN_VALIDATION_ERROR(
        "Expected chain root to match one of the following branch types with optional extensions:"
        "\n%sInstead found: %s",
        Validator::ToString(), ToString());
}

// Validates that an unpacked chain retrieved via ValidateAndUnpack contains a subset of the
// Allowed extensions. If there are any other extensions, returns an error.
//
// Example usage:
//     Unpacked<T> u;
//     DAWN_TRY_ASSIGN(u, ValidateAndUnpack(desc));
//     DAWN_TRY(u.ValidateSubset<Ext1>());
//
// Even though "valid" extensions on descriptor may include both Ext1 and Ext2, ValidateSubset
// will further enforce that Ext2 is not on the chain in the example above.
template <typename T>
template <typename... Allowed>
MaybeError Unpacked<T>::ValidateSubset() const {
    return detail::SubsetValidator<Unpacked<T>, Allowed...>::Validate(*this, mBitset);
}
template <typename T>
template <typename... Allowed>
MaybeError UnpackedOut<T>::ValidateSubset() const {
    return detail::SubsetValidator<UnpackedOut<T>, Allowed...>::Validate(*this, mBitset);
}

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CHAINUTILS_H_
