// Copyright 2022 The Dawn Authors
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

#ifndef DAWN_NATIVE_TRAITS_SERIALIZETINT_IMPL_H_
#define DAWN_NATIVE_TRAITS_SERIALIZETINT_IMPL_H_

#include "dawn/common/traits/Serialize.h"

#include <tint/tint.h>

namespace dawn {

    template <>
    struct SerializeTraits<tint::sem::BindingPoint> {
        static constexpr size_t Size(const tint::sem::BindingPoint& value) {
            return sizeof(tint::sem::BindingPoint);
        }

        static void Write(char** ptr, const tint::sem::BindingPoint& value) {
            memcpy(*ptr, &value, sizeof(tint::sem::BindingPoint));
            *ptr += sizeof(tint::sem::BindingPoint);
        }
    };

    template <>
    struct SerializeTraits<tint::transform::BindingPoints> {
        static constexpr size_t Size(const tint::transform::BindingPoints& value) {
            return sizeof(tint::transform::BindingPoints);
        }

        static void Write(char** ptr, const tint::transform::BindingPoints& value) {
            memcpy(*ptr, &value, sizeof(tint::transform::BindingPoints));
            *ptr += sizeof(tint::transform::BindingPoints);
        }
    };

    // TODO
    template <>
    struct SerializeTraits<tint::transform::VertexPulling::Config> {
        static constexpr size_t Size(const tint::transform::VertexPulling::Config& cfg) {
            return 0;
        }

        static void Write(char** ptr, const tint::transform::VertexPulling::Config& cfg) {
        }
    };

    template <>
    struct SerializeTraits<const tint::Program*> {
        // TODO: Avoid double serialization.
        // TODO(tint:1180): Consider using a binary serialization of the tint AST for a more
        // compact representation.
        static size_t Size(const tint::Program* program) {
            auto result = tint::writer::wgsl::Generate(program, tint::writer::wgsl::Options{});
            ASSERT(result.success);
            return SerializeTraits<size_t>::Size(result.wgsl.length()) + result.wgsl.length();
        }

        static void Write(char** ptr, const tint::Program* program) {
            auto result = tint::writer::wgsl::Generate(program, tint::writer::wgsl::Options{});
            ASSERT(result.success);

            SerializeTraits<size_t>::Write(ptr, result.wgsl.length());

            memcpy(*ptr, result.wgsl.data(), result.wgsl.length());
            *ptr += result.wgsl.length();
        }
    };

}  // namespace dawn

#endif  // DAWN_NATIVE_TRAITS_SERIALIZETINT_IMPL_H_