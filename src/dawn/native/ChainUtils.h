// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_CHAINUTILS_H_
#define SRC_DAWN_NATIVE_CHAINUTILS_H_

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/Error.h"

namespace dawn::native {

namespace detail {

// Labeling template types for readability.
template <typename B>
struct Branch {};
template <typename... Exts>
struct Extensions {};

// Tuple type of a Branch and a corresponding Extensions.
template <typename B, typename Exts>
struct BranchExtensions {};

// Typelist type used to specify a list of acceptable BranchExtensions.
template <typename... BranchExts>
struct BranchExtensionsList {};

// Root specializations for branching chained struct types.
template <typename Root>
struct RootBranchExtensions {};

template <typename Unpacked, typename...>
struct OneRootBranchExtensionValidator {};
template <typename Unpacked, typename B, typename... Exts>
struct OneRootBranchExtensionValidator<Unpacked, BranchExtensions<Branch<B>, Extensions<Exts...>>> {
    static void Validate(const Unpacked& unpacked, std::vector<wgpu::SType>& matches) {
        // Early out if we already hit a previous match and avoid any additional computation.
        if (matches.size() > 0) {
            return;
        }
        if (std::get<const B*>(unpacked) != nullptr) {
            // Create a set of the valid STypes that we are allowed to see in this branch. Note we
            // add the branch SType also because it's a given that the branch chained struct is
            // allowed.
            std::unordered_set<wgpu::SType> stypes = STypesFor<Exts...>();
            stypes.insert(STypeFor<B>);

            bool valid = std::apply(
                [&](const auto*... args) {
                    return (([&](const auto* arg) {
                                return arg == nullptr || stypes.count(arg->sType);
                            }(args)) &&
                            ...);
                },
                unpacked);
            if (valid) {
                matches.push_back(STypeFor<B>);
            }
        }
    }

    static std::string ToString() {
        std::string result = "[ " + STypesToString<B>();
        if constexpr (sizeof...(Exts) > 0) {
            result += ": " + STypesToString<Exts...>();
        }
        result += " ]";
        return result;
    }
};

template <typename Unpacked, typename List>
struct RootBranchExtensionsValidator {};
template <typename Unpacked, typename... BranchExts>
struct RootBranchExtensionsValidator<Unpacked, BranchExtensionsList<BranchExts...>> {
    static void Validate(const Unpacked& unpacked, std::vector<wgpu::SType>& matches) {
        return (
            (OneRootBranchExtensionValidator<Unpacked, BranchExts>::Validate(unpacked, matches)),
            ...);
    }

    static std::string ToString() {
        return "(" +
               ((OneRootBranchExtensionValidator<Unpacked, BranchExts>::ToString() + ", ") + ...) +
               ")";
    }
};

}  // namespace detail

template <typename Unpacked>
ResultOrError<wgpu::SType> ValidateRootBranchExtensions(const Unpacked& unpacked) {
    using RootBranchExts = detail::RootBranchExtensions<typename RootTypeFor<Unpacked>::Type>::List;

    std::vector<wgpu::SType> matches;
    detail::RootBranchExtensionsValidator<Unpacked, RootBranchExts>::Validate(unpacked, matches);
    if (matches.size() == 1) {
        return std::move(matches[0]);
    }

    return DAWN_VALIDATION_ERROR(
        "Expected chain root to match one of the following branch types with optional extensions: "
        "%s. Instead found: %s",
        detail::RootBranchExtensionsValidator<Unpacked, RootBranchExts>::ToString(),
        detail::UnpackedChainToString(unpacked));
}

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_CHAINUTILS_H_
