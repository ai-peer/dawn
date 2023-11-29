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

#include "absl/strings/str_format.h"
#include "dawn/common/Math.h"
#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/Error.h"

namespace dawn::native {

// Tuple type of a Branch and an optional list of corresponding Extensions.
template <typename B, typename... Exts>
struct Branch;

namespace detail {

template <typename UnpackedTuple, typename... Exts>
struct UnpackedTupleBitsetForExts {
    // Currently using an internal 64-bit unsigned int for internal representation. This is
    // necessary because std::bitset::operator| is not constexpr until C++23.
    static constexpr auto value = std::bitset<std::tuple_size_v<UnpackedTuple>>(
        ((uint64_t(1) << UnpackedTupleIndexOf<UnpackedTuple, Exts>::value) | ...));
};

template <typename UnpackedT, typename...>
struct OneBranchValidator;
template <typename UnpackedT, typename B, typename... Exts>
struct OneBranchValidator<UnpackedT, Branch<B, Exts...>> {
    using UnpackedTuple = typename UnpackedT::TupleType;

    static bool Validate(const UnpackedT& unpacked,
                         const std::bitset<std::tuple_size_v<UnpackedTuple>>& actual,
                         wgpu::SType& match) {
        // Only check the full bitset when the main branch matches.
        if (unpacked.template Get<B>() != nullptr) {
            // Allowed set of extensions includes the branch root as well.
            constexpr auto allowed = UnpackedTupleBitsetForExts<UnpackedTuple, B, Exts...>::value;

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
    using UnpackedTuple = typename UnpackedT::TupleType;

    static bool Validate(const UnpackedT& unpacked, wgpu::SType& match) {
        return ((OneBranchValidator<UnpackedT, Branches>::Validate(unpacked, unpacked.Bitset(),
                                                                   match)) ||
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
    using UnpackedTuple = typename Unpacked<T>::TupleType;

    static std::string ToString() {
        return absl::StrFormat("[ %s ]", STypesToString<Allowed...>());
    }

    static MaybeError Validate(const Unpacked<T>& unpacked) {
        // Allowed set of extensions includes the branch root as well.
        constexpr auto allowed =
            detail::UnpackedTupleBitsetForExts<UnpackedTuple, std::add_const_t<Allowed>*...>::value;
        if (!IsSubset(unpacked.Bitset(), allowed)) {
            return DAWN_VALIDATION_ERROR(
                "Expected extension set to be a subset of:\n%sInstead found: %s", ToString(),
                detail::UnpackedChainToString(unpacked));
        }
        return {};
    }
};
template <typename T, typename... Allowed>
struct SubsetValidator<UnpackedOut<T>, Allowed...> {
    using UnpackedTuple = typename UnpackedOut<T>::TupleType;

    static std::string ToString() {
        return absl::StrFormat("[ %s ]", STypesToString<Allowed...>());
    }

    static MaybeError Validate(const UnpackedOut<T>& unpacked) {
        // Allowed set of extensions includes the branch root as well.
        constexpr auto allowed =
            detail::UnpackedTupleBitsetForExts<UnpackedTuple, Allowed*...>::value;
        if (!IsSubset(unpacked.Bitset(), allowed)) {
            return DAWN_VALIDATION_ERROR(
                "Expected extension set to be a subset of:\n%sInstead found: %s", ToString(),
                detail::UnpackedChainToString(unpacked));
        }
        return {};
    }
};

}  // namespace detail

// Validates that an unpacked chain retrieved via ValidateAndUnpack matches a valid "branch",
// where a "branch" is defined as a required "root" extension and optional follow-up
// extensions.
//
// Returns the wgpu::SType associated with the "root" extension of a "branch" if matched, otherwise
// returns an error.
//
// Example usage:
//     Unpacked<T> u;
//     DAWN_TRY_ASSIGN(u, ValidateAndUnpack(desc));
//     wgpu::SType rootType;
//     DAWN_TRY_ASSIGN(rootType,
//         ValidateBranches<Branch<Root1>, Branch<Root2, R2Ext1>>(u));
//     switch (rootType) {
//         case Root1: {
//             <do something>
//         }
//         case Root2: {
//             R2Ext1 ext = std::get<const R2Ext1*>(u);
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
template <typename... Branches, typename UnpackedT>
ResultOrError<wgpu::SType> ValidateUnpackedBranches(const UnpackedT& unpacked) {
    using Validator = detail::BranchesValidator<UnpackedT, Branches...>;

    wgpu::SType match = wgpu::SType::Invalid;
    if (Validator::Validate(unpacked, match)) {
        return match;
    }
    return DAWN_VALIDATION_ERROR(
        "Expected chain root to match one of the following branch types with optional extensions:"
        "\n%sInstead found: %s",
        Validator::ToString(), detail::UnpackedChainToString(unpacked));
}

// Validates that an unpacked chain retrieved via ValidateAndUnpack contains a subset of the
// Allowed extensions. If there are any other extensions, returns an error.
template <typename... Allowed, typename UnpackedT>
MaybeError ValidateUnpackedSubset(const UnpackedT& unpacked) {
    return detail::SubsetValidator<UnpackedT, Allowed...>::Validate(unpacked);
}

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CHAINUTILS_H_
