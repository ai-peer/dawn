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

#ifndef SRC_TINT_INTERP_TEXTURE_H_
#define SRC_TINT_INTERP_TEXTURE_H_

#include <cstdint>
// #include <vector>

// #include "tint/ast/diagnostic_control.h"
// #include "tint/source.h"
// #include "tint/type/address_space.h"
// #include "tint/utils/block_allocator.h"

// Forward Declarations
// namespace tint::constant {
// class Value;
// }  // namespace tint::constant
// namespace tint::interp {
// class Memory;
// class ShaderExecutor;
// }  // namespace tint::interp
// namespace tint::type {
// class Type;
// }  // namespace tint::type

namespace tint::interp {

/// A Texture objects represents a texture allocation.
class Texture {
  public:
    /// Constructor
    explicit Texture();

    /// Destructor
    ~Texture();

  private:
};

/// A TextureView object represents a view into a texture allocation.
class TextureView {
  public:
    /// Constructor
    explicit TextureView();

    /// Destructor
    ~TextureView();

    /// TODO
    /// @returns the width of the texture in texels
    uint32_t Width() const { return 17; }

  private:
};

}  // namespace tint::interp

#endif  // SRC_TINT_INTERP_TEXTURE_H_
