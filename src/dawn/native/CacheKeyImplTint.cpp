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

#include "dawn/native/CacheKey.h"

#include "tint/tint.h"

namespace dawn::native {

// static
template <>
void CacheKeySerializer<tint::Program>::Serialize(CacheKey* key, const tint::Program& p) {
#if TINT_BUILD_WGSL_WRITER
    tint::writer::wgsl::Options options{};
    key->Record(tint::writer::wgsl::Generate(&p, options).wgsl);
#else
    // TODO(crbug.com/dawn/1481): We shouldn't need to write back to WGSL if we have a CacheKey
    // built from the initial shader module input. Then, we would never need to parse the program
    // and write back out to WGSL.
    UNREACHABLE();
#endif
}

// static
template <>
void CacheKeySerializer<tint::transform::BindingPoints>::Serialize(
    CacheKey* key,
    const tint::transform::BindingPoints& points) {
    static_assert(offsetof(tint::transform::BindingPoints, plane_1) == 0,
                  "Please update serialization for tint::transform::BindingPoints");
    static_assert(offsetof(tint::transform::BindingPoints, params) == 8,
                  "Please update serialization for tint::transform::BindingPoints");
    static_assert(sizeof(tint::transform::BindingPoints) == 16,
                  "Please update serialization for tint::transform::BindingPoints");
    key->Record(points.plane_1, points.params);
}

// static
template <>
void CacheKeySerializer<tint::sem::BindingPoint>::Serialize(CacheKey* key,
                                                            const tint::sem::BindingPoint& p) {
    static_assert(offsetof(tint::sem::BindingPoint, group) == 0,
                  "Please update serialization for tint::sem::BindingPoint");
    static_assert(offsetof(tint::sem::BindingPoint, binding) == 4,
                  "Please update serialization for tint::sem::BindingPoint");
    static_assert(sizeof(tint::sem::BindingPoint) == 8,
                  "Please update serialization for tint::sem::BindingPoint");
    key->Record(p.group, p.binding);
}

}  // namespace dawn::native
