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

namespace tint::ir {

/// An operation in the IR.
class Op {
  public:
    /// The kind of operation
    enum class Kind {
        kLoadConstant = 1,
        kLoad,
        kStore,

        kAdd,
        kSub,
        kMul,
        kDiv,
        kRem,
        kNegate,

        kCall,
    };

    /// An operation data element
    struct Data {
        /// The register to read the lhs from
        Register lhs;
        /// The register to read the rhs from
        Register rhs;
    };

    /// Constructor
    /// @param k the kind of operation
    explicit Op(Kind k);

    /// Returns true if the op holds a constant value
    bool HasConstant() const;
    /// Returns true if the op holds a data value
    bool HasData() const;

    /// @returns the constant value
    const Constant& GetConstant() const;
    /// @returns the data value
    const Data& GetData() const;

    /// The kind of operation
    Kind kind;
    /// The register to store the result into
    Register result;

    /// The arguments to this operator. Either two registers or a constant value.
    std::variant<Data, Constant> args;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_OP_H_
