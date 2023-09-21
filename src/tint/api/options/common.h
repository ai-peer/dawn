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

#ifndef SRC_TINT_API_OPTIONS_COMMON_H_
#define SRC_TINT_API_OPTIONS_COMMON_H_

#include "src/tint/api/options/external_texture.h"

namespace tint::options {

struct Common {
    /// Set to `true` to disable workgroup memory zero initialization
    bool disable_workgroup_init = false;

    /// Set to `true` to disable software robustness that prevents out-of-bounds accesses.
    bool disable_robustness = false;

    /// Set to `true` to generate via the Tint IR instead of from the AST.
    bool use_tint_ir = false;

    /// Options used in the binding mappings for external textures
    ExternalTextureOptions external_texture_options = {};
};

}  // namespace tint::options

#endif  // SRC_TINT_API_OPTIONS_COMMON_H_
