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

#include "src/tint/ir/binary.h"
#include "src/tint/debug.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::Binary);

namespace tint::ir {

Binary::Binary(uint32_t id, Kind kind, const type::Type* ty, Value* lhs, Value* rhs)
    : Base(id, ty), kind_(kind), lhs_(lhs), rhs_(rhs) {
    TINT_ASSERT(IR, lhs_);
    TINT_ASSERT(IR, rhs_);
    lhs_->AddUsage(this);
    rhs_->AddUsage(this);
}

Binary::~Binary() = default;

utils::StringStream& Binary::ToInstruction(utils::StringStream& out) const {
    ToValue(out) << " = ";

    switch (GetKind()) {
        case Binary::Kind::kAdd:
            out << "add";
            break;
        case Binary::Kind::kSubtract:
            out << "sub";
            break;
        case Binary::Kind::kMultiply:
            out << "mul";
            break;
        case Binary::Kind::kDivide:
            out << "div";
            break;
        case Binary::Kind::kModulo:
            out << "mod";
            break;
        case Binary::Kind::kAnd:
            out << "bitwise_and";
            break;
        case Binary::Kind::kOr:
            out << "bitwise_or";
            break;
        case Binary::Kind::kXor:
            out << "bitwise_xor";
            break;
        case Binary::Kind::kLogicalAnd:
            out << "logical_and";
            break;
        case Binary::Kind::kLogicalOr:
            out << "logical_or";
            break;
        case Binary::Kind::kEqual:
            out << "eq";
            break;
        case Binary::Kind::kNotEqual:
            out << "not_eq";
            break;
        case Binary::Kind::kLessThan:
            out << "less_than";
            break;
        case Binary::Kind::kGreaterThan:
            out << "greater_than";
            break;
        case Binary::Kind::kLessThanEqual:
            out << "less_than_eq";
            break;
        case Binary::Kind::kGreaterThanEqual:
            out << "greater_than_eq";
            break;
        case Binary::Kind::kShiftLeft:
            out << "shift_left";
            break;
        case Binary::Kind::kShiftRight:
            out << "shift_right";
            break;
    }
    out << " ";
    lhs_->ToValue(out) << ", ";
    rhs_->ToValue(out);
    return out;
}

}  // namespace tint::ir
