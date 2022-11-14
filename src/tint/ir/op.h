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

#ifndef SRC_TINT_IR_OP_H_
#define SRC_TINT_IR_OP_H_

#include <variant>

#include "src/tint/ir/constant.h"
#include "src/tint/ir/register.h"
#include "src/tint/utils/vector.h"

namespace tint::ir {

/// An operation in the IR.
class Op {
  public:
    /// The kind of operation
    enum class Kind {
        kNone = 0,

        kLoadConstant,
        kLoad,
        kStore,

        kAnd,
        kOr,
        kXor,
        kLogicalAnd,
        kLogicalOr,
        kEqual,
        kNotEqual,
        kLessThan,
        kLessThanEqual,
        kGreaterThan,
        kGreaterThanEqual,
        kShiftLeft,
        kShiftRight,

        kAdd,
        kSubtract,
        kMultiply,
        kDivide,
        kModulo,

        kCall,
    };

    /// An operation data element
    struct Data {
        /// Data constant or data register
        std::variant<Register, Constant> value;

        /// @returns true if the op holds a constant value
        bool HasConstant() const { return std::holds_alternative<Constant>(value); }
        /// @returns true if the op holds a register value
        bool HasRegister() const { return std::holds_alternative<Register>(value); }

        /// @returns the constant value
        const Constant& GetConstant() const { return std::get<Constant>(value); }
        /// @returns the register value
        const Register& GetRegister() const { return std::get<Register>(value); }
    };

    /// Constructor
    /// @param k the kind of operation
    explicit Op(Kind k);

    /// @returns true if Op has a result
    bool HasResult() const { return result.id > 0; }

    /// The kind of operation
    Kind kind;

    /// The register to store the result into
    Register result;

    /// The arguments to this operator. Either two registers or a constant value.
    utils::Vector<Data, 2> args;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_OP_H_
