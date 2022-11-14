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

#ifndef SRC_TINT_IR_CONSTANT_H_
#define SRC_TINT_IR_CONSTANT_H_

#include <cstdint>
#include <variant>

#include "src/tint/ir/register.h"

namespace tint::ir {

/// A constant value in the IR
struct Constant {
    /// The kind of constant
    enum class Kind {
      /// Boolean constant
        kBool,
        /// F16 constant
        kF16,
        /// F32 constant
        kF32,
        /// I32 constant
        kI32,
        /// U32 constant
        kU32,
    };

    /// @returns true if the constant holds a bool
    bool IsBool() const { return kind == Kind::kBool; }
    /// @returns true if the constant holds a f16
    bool IsF16() const { return kind == Kind::kF16; }
    /// @returns true if the constant holds a f32
    bool IsF32() const { return kind == Kind::kF32; }
    /// @returns true if the constant holds an i32
    bool IsI32() const { return kind == Kind::kI32; }
    /// @returns true if the constant holds a u32
    bool IsU32() const { return kind == Kind::kU32; }

    /// @returns the bool value
    bool AsBool() const { return std::get<bool>(value); }
    /// @returns the float value
    float AsF32() const { return static_cast<float>(std::get<double>(value)); }
    /// @returns the int32 value
    int32_t AsI32() const { return static_cast<int32_t>(std::get<int64_t>(value)); }
    /// @returns the uint32 value
    uint32_t AsU32() const { return static_cast<uint32_t>(std::get<int64_t>(value)); }

    /// The kind of the constant
    Kind kind;
    /// The constant value
    std::variant<bool, double, int64_t> value;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_CONSTANT_H_
