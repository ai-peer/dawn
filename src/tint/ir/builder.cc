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

#include "src/tint/ir/builder.h"

#include <utility>

#include "src/tint/ir/builder_impl.h"

namespace tint::ir {

Builder::Builder() {}

Builder::Builder(Module&& mod) : ir(std::move(mod)) {}

Builder::~Builder() = default;

Block* Builder::CreateBlock() {
    return ir.flow_nodes.Create<Block>();
}

Terminator* Builder::CreateTerminator() {
    return ir.flow_nodes.Create<Terminator>();
}

Function* Builder::CreateFunction() {
    auto* ir_func = ir.flow_nodes.Create<Function>();
    ir_func->start_target = CreateBlock();
    ir_func->end_target = CreateTerminator();

    // Function is always branching into the start target
    ir_func->start_target->inbound_branches.Push(ir_func);

    return ir_func;
}

If* Builder::CreateIf() {
    auto* ir_if = ir.flow_nodes.Create<If>();
    ir_if->true_.target = CreateBlock();
    ir_if->false_.target = CreateBlock();
    ir_if->merge.target = CreateBlock();

    // An if always branches to both the true and false block.
    ir_if->true_.target->inbound_branches.Push(ir_if);
    ir_if->false_.target->inbound_branches.Push(ir_if);

    return ir_if;
}

Loop* Builder::CreateLoop() {
    auto* ir_loop = ir.flow_nodes.Create<Loop>();
    ir_loop->start.target = CreateBlock();
    ir_loop->continuing.target = CreateBlock();
    ir_loop->merge.target = CreateBlock();

    // A loop always branches to the start block.
    ir_loop->start.target->inbound_branches.Push(ir_loop);

    return ir_loop;
}

Switch* Builder::CreateSwitch() {
    auto* ir_switch = ir.flow_nodes.Create<Switch>();
    ir_switch->merge.target = CreateBlock();
    return ir_switch;
}

Block* Builder::CreateCase(Switch* s, utils::VectorRef<Switch::CaseSelector> selectors) {
    s->cases.Push(Switch::Case{selectors, {CreateBlock(), utils::Empty}});

    Block* b = s->cases.Back().start.target->As<Block>();
    // Switch branches into the case block
    b->inbound_branches.Push(s);
    return b;
}

void Builder::Branch(Block* from, FlowNode* to, utils::VectorRef<Value*> args) {
    TINT_ASSERT(IR, from);
    TINT_ASSERT(IR, to);
    from->branch.target = to;
    from->branch.args = args;
    to->inbound_branches.Push(from);
}

Temp::Id Builder::AllocateTempId() {
    return next_temp_id++;
}

Binary* Builder::CreateBinary(Binary::Kind kind, const type::Type* type, Value* lhs, Value* rhs) {
    return ir.instructions.Create<ir::Binary>(kind, Temp(type), lhs, rhs);
}

Binary* Builder::And(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kAnd, type, lhs, rhs);
}

Binary* Builder::Or(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kOr, type, lhs, rhs);
}

Binary* Builder::Xor(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kXor, type, lhs, rhs);
}

Binary* Builder::LogicalAnd(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kLogicalAnd, type, lhs, rhs);
}

Binary* Builder::LogicalOr(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kLogicalOr, type, lhs, rhs);
}

Binary* Builder::Equal(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kEqual, type, lhs, rhs);
}

Binary* Builder::NotEqual(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kNotEqual, type, lhs, rhs);
}

Binary* Builder::LessThan(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kLessThan, type, lhs, rhs);
}

Binary* Builder::GreaterThan(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kGreaterThan, type, lhs, rhs);
}

Binary* Builder::LessThanEqual(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kLessThanEqual, type, lhs, rhs);
}

Binary* Builder::GreaterThanEqual(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kGreaterThanEqual, type, lhs, rhs);
}

Binary* Builder::ShiftLeft(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kShiftLeft, type, lhs, rhs);
}

Binary* Builder::ShiftRight(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kShiftRight, type, lhs, rhs);
}

Binary* Builder::Add(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kAdd, type, lhs, rhs);
}

Binary* Builder::Subtract(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kSubtract, type, lhs, rhs);
}

Binary* Builder::Multiply(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kMultiply, type, lhs, rhs);
}

Binary* Builder::Divide(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kDivide, type, lhs, rhs);
}

Binary* Builder::Modulo(const type::Type* type, Value* lhs, Value* rhs) {
    return CreateBinary(Binary::Kind::kModulo, type, lhs, rhs);
}

ir::Bitcast* Builder::Bitcast(const type::Type* type, Value* val) {
    return ir.instructions.Create<ir::Bitcast>(Temp(type), val);
}

ir::UserCall* Builder::UserCall(const type::Type* type,
                                Symbol name,
                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::UserCall>(Temp(type), name, args);
}

ir::ValueConversion* Builder::ValueConversion(const type::Type* to,
                                              const type::Type* from,
                                              utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::ValueConversion>(Temp(to), from, args);
}

ir::ValueConstructor* Builder::ValueConstructor(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::ValueConstructor>(Temp(to), args);
}

ir::builtins::Abs* Builder::Abs(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Abs>(Temp(to), args);
}
ir::builtins::Acos* Builder::Acos(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Acos>(Temp(to), args);
}
ir::builtins::Acosh* Builder::Acosh(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Acosh>(Temp(to), args);
}
ir::builtins::All* Builder::All(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::All>(Temp(to), args);
}
ir::builtins::Any* Builder::Any(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Any>(Temp(to), args);
}
ir::builtins::ArrayLength* Builder::ArrayLength(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::ArrayLength>(Temp(to), args);
}
ir::builtins::Asin* Builder::Asin(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Asin>(Temp(to), args);
}
ir::builtins::Asinh* Builder::Asinh(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Asinh>(Temp(to), args);
}
ir::builtins::Atan* Builder::Atan(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Atan>(Temp(to), args);
}
ir::builtins::Atan2* Builder::Atan2(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Atan2>(Temp(to), args);
}
ir::builtins::Atanh* Builder::Atanh(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Atanh>(Temp(to), args);
}
ir::builtins::Ceil* Builder::Ceil(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Ceil>(Temp(to), args);
}
ir::builtins::Clamp* Builder::Clamp(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Clamp>(Temp(to), args);
}
ir::builtins::Cos* Builder::Cos(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Cos>(Temp(to), args);
}
ir::builtins::Cosh* Builder::Cosh(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Cosh>(Temp(to), args);
}
ir::builtins::CountLeadingZeros* Builder::CountLeadingZeros(const type::Type* to,
                                                            utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::CountLeadingZeros>(Temp(to), args);
}
ir::builtins::CountOneBits* Builder::CountOneBits(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::CountOneBits>(Temp(to), args);
}
ir::builtins::CountTrailingZeros* Builder::CountTrailingZeros(const type::Type* to,
                                                              utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::CountTrailingZeros>(Temp(to), args);
}
ir::builtins::Cross* Builder::Cross(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Cross>(Temp(to), args);
}
ir::builtins::Degrees* Builder::Degrees(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Degrees>(Temp(to), args);
}
ir::builtins::Determinant* Builder::Determinant(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Determinant>(Temp(to), args);
}
ir::builtins::Distance* Builder::Distance(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Distance>(Temp(to), args);
}
ir::builtins::Dot* Builder::Dot(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Dot>(Temp(to), args);
}
ir::builtins::Dot4I8Packed* Builder::Dot4I8Packed(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Dot4I8Packed>(Temp(to), args);
}
ir::builtins::Dot4U8Packed* Builder::Dot4U8Packed(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Dot4U8Packed>(Temp(to), args);
}
ir::builtins::Dpdx* Builder::Dpdx(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Dpdx>(Temp(to), args);
}
ir::builtins::DpdxCoarse* Builder::DpdxCoarse(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::DpdxCoarse>(Temp(to), args);
}
ir::builtins::DpdxFine* Builder::DpdxFine(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::DpdxFine>(Temp(to), args);
}
ir::builtins::Dpdy* Builder::Dpdy(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Dpdy>(Temp(to), args);
}
ir::builtins::DpdyCoarse* Builder::DpdyCoarse(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::DpdyCoarse>(Temp(to), args);
}
ir::builtins::DpdyFine* Builder::DpdyFine(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::DpdyFine>(Temp(to), args);
}
ir::builtins::Exp* Builder::Exp(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Exp>(Temp(to), args);
}
ir::builtins::Exp2* Builder::Exp2(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Exp2>(Temp(to), args);
}
ir::builtins::ExtractBits* Builder::ExtractBits(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::ExtractBits>(Temp(to), args);
}
ir::builtins::FaceForward* Builder::FaceForward(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::FaceForward>(Temp(to), args);
}
ir::builtins::FirstLeadingBit* Builder::FirstLeadingBit(const type::Type* to,
                                                        utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::FirstLeadingBit>(Temp(to), args);
}
ir::builtins::FirstTrailingBit* Builder::FirstTrailingBit(const type::Type* to,
                                                          utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::FirstTrailingBit>(Temp(to), args);
}
ir::builtins::Floor* Builder::Floor(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Floor>(Temp(to), args);
}
ir::builtins::Fma* Builder::Fma(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Fma>(Temp(to), args);
}
ir::builtins::Fract* Builder::Fract(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Fract>(Temp(to), args);
}
ir::builtins::Frexp* Builder::Frexp(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Frexp>(Temp(to), args);
}
ir::builtins::Fwidth* Builder::Fwidth(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Fwidth>(Temp(to), args);
}
ir::builtins::FwidthCoarse* Builder::FwidthCoarse(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::FwidthCoarse>(Temp(to), args);
}
ir::builtins::FwidthFine* Builder::FwidthFine(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::FwidthFine>(Temp(to), args);
}
ir::builtins::InsertBits* Builder::InsertBits(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::InsertBits>(Temp(to), args);
}
ir::builtins::InverseSqrt* Builder::InverseSqrt(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::InverseSqrt>(Temp(to), args);
}
ir::builtins::Ldexp* Builder::Ldexp(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Ldexp>(Temp(to), args);
}
ir::builtins::Length* Builder::Length(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Length>(Temp(to), args);
}
ir::builtins::Log* Builder::Log(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Log>(Temp(to), args);
}
ir::builtins::Log2* Builder::Log2(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Log2>(Temp(to), args);
}
ir::builtins::Max* Builder::Max(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Max>(Temp(to), args);
}
ir::builtins::Min* Builder::Min(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Min>(Temp(to), args);
}
ir::builtins::Mix* Builder::Mix(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Mix>(Temp(to), args);
}
ir::builtins::Modf* Builder::Modf(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Modf>(Temp(to), args);
}
ir::builtins::Normalize* Builder::Normalize(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Normalize>(Temp(to), args);
}
ir::builtins::Pack2X16Float* Builder::Pack2X16Float(const type::Type* to,
                                                    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Pack2X16Float>(Temp(to), args);
}
ir::builtins::Pack2X16Snorm* Builder::Pack2X16Snorm(const type::Type* to,
                                                    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Pack2X16Snorm>(Temp(to), args);
}
ir::builtins::Pack2X16Unorm* Builder::Pack2X16Unorm(const type::Type* to,
                                                    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Pack2X16Unorm>(Temp(to), args);
}
ir::builtins::Pack4X8Snorm* Builder::Pack4X8Snorm(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Pack4X8Snorm>(Temp(to), args);
}
ir::builtins::Pack4X8Unorm* Builder::Pack4X8Unorm(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Pack4X8Unorm>(Temp(to), args);
}
ir::builtins::Pow* Builder::Pow(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Pow>(Temp(to), args);
}
ir::builtins::QuantizeToF16* Builder::QuantizeToF16(const type::Type* to,
                                                    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::QuantizeToF16>(Temp(to), args);
}
ir::builtins::Radians* Builder::Radians(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Radians>(Temp(to), args);
}
ir::builtins::Reflect* Builder::Reflect(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Reflect>(Temp(to), args);
}
ir::builtins::Refract* Builder::Refract(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Refract>(Temp(to), args);
}
ir::builtins::ReverseBits* Builder::ReverseBits(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::ReverseBits>(Temp(to), args);
}
ir::builtins::Round* Builder::Round(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Round>(Temp(to), args);
}
ir::builtins::Saturate* Builder::Saturate(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Saturate>(Temp(to), args);
}
ir::builtins::Select* Builder::Select(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Select>(Temp(to), args);
}
ir::builtins::Sign* Builder::Sign(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Sign>(Temp(to), args);
}
ir::builtins::Sin* Builder::Sin(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Sin>(Temp(to), args);
}
ir::builtins::Sinh* Builder::Sinh(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Sinh>(Temp(to), args);
}
ir::builtins::Smoothstep* Builder::Smoothstep(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Smoothstep>(Temp(to), args);
}
ir::builtins::Sqrt* Builder::Sqrt(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Sqrt>(Temp(to), args);
}
ir::builtins::Step* Builder::Step(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Step>(Temp(to), args);
}
ir::builtins::StorageBarrier* Builder::StorageBarrier(const type::Type* to,
                                                      utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::StorageBarrier>(Temp(to), args);
}
ir::builtins::Tan* Builder::Tan(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Tan>(Temp(to), args);
}
ir::builtins::Tanh* Builder::Tanh(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Tanh>(Temp(to), args);
}
ir::builtins::Transpose* Builder::Transpose(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Transpose>(Temp(to), args);
}
ir::builtins::Trunc* Builder::Trunc(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Trunc>(Temp(to), args);
}
ir::builtins::Unpack2X16Float* Builder::Unpack2X16Float(const type::Type* to,
                                                        utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Unpack2X16Float>(Temp(to), args);
}
ir::builtins::Unpack2X16Snorm* Builder::Unpack2X16Snorm(const type::Type* to,
                                                        utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Unpack2X16Snorm>(Temp(to), args);
}
ir::builtins::Unpack2X16Unorm* Builder::Unpack2X16Unorm(const type::Type* to,
                                                        utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Unpack2X16Unorm>(Temp(to), args);
}
ir::builtins::Unpack4X8Snorm* Builder::Unpack4X8Snorm(const type::Type* to,
                                                      utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Unpack4X8Snorm>(Temp(to), args);
}
ir::builtins::Unpack4X8Unorm* Builder::Unpack4X8Unorm(const type::Type* to,
                                                      utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::Unpack4X8Unorm>(Temp(to), args);
}
ir::builtins::WorkgroupBarrier* Builder::WorkgroupBarrier(const type::Type* to,
                                                          utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::WorkgroupBarrier>(Temp(to), args);
}
ir::builtins::WorkgroupUniformLoad* Builder::WorkgroupUniformLoad(const type::Type* to,
                                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::WorkgroupUniformLoad>(Temp(to), args);
}
ir::builtins::TextureDimensions* Builder::TextureDimensions(const type::Type* to,
                                                            utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureDimensions>(Temp(to), args);
}
ir::builtins::TextureGather* Builder::TextureGather(const type::Type* to,
                                                    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureGather>(Temp(to), args);
}
ir::builtins::TextureGatherCompare* Builder::TextureGatherCompare(const type::Type* to,
                                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureGatherCompare>(Temp(to), args);
}
ir::builtins::TextureNumLayers* Builder::TextureNumLayers(const type::Type* to,
                                                          utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureNumLayers>(Temp(to), args);
}
ir::builtins::TextureNumLevels* Builder::TextureNumLevels(const type::Type* to,
                                                          utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureNumLevels>(Temp(to), args);
}
ir::builtins::TextureNumSamples* Builder::TextureNumSamples(const type::Type* to,
                                                            utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureNumSamples>(Temp(to), args);
}
ir::builtins::TextureSample* Builder::TextureSample(const type::Type* to,
                                                    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSample>(Temp(to), args);
}
ir::builtins::TextureSampleBias* Builder::TextureSampleBias(const type::Type* to,
                                                            utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSampleBias>(Temp(to), args);
}
ir::builtins::TextureSampleCompare* Builder::TextureSampleCompare(const type::Type* to,
                                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSampleCompare>(Temp(to), args);
}
ir::builtins::TextureSampleCompareLevel* Builder::TextureSampleCompareLevel(
    const type::Type* to,
    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSampleCompareLevel>(Temp(to), args);
}
ir::builtins::TextureSampleGrad* Builder::TextureSampleGrad(const type::Type* to,
                                                            utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSampleGrad>(Temp(to), args);
}
ir::builtins::TextureSampleLevel* Builder::TextureSampleLevel(const type::Type* to,
                                                              utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSampleLevel>(Temp(to), args);
}
ir::builtins::TextureSampleBaseClampToEdge* Builder::TextureSampleBaseClampToEdge(
    const type::Type* to,
    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureSampleBaseClampToEdge>(Temp(to), args);
}
ir::builtins::TextureStore* Builder::TextureStore(const type::Type* to,
                                                  utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureStore>(Temp(to), args);
}
ir::builtins::TextureLoad* Builder::TextureLoad(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::TextureLoad>(Temp(to), args);
}
ir::builtins::AtomicLoad* Builder::AtomicLoad(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicLoad>(Temp(to), args);
}
ir::builtins::AtomicStore* Builder::AtomicStore(const type::Type* to,
                                                utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicStore>(Temp(to), args);
}
ir::builtins::AtomicAdd* Builder::AtomicAdd(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicAdd>(Temp(to), args);
}
ir::builtins::AtomicSub* Builder::AtomicSub(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicSub>(Temp(to), args);
}
ir::builtins::AtomicMax* Builder::AtomicMax(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicMax>(Temp(to), args);
}
ir::builtins::AtomicMin* Builder::AtomicMin(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicMin>(Temp(to), args);
}
ir::builtins::AtomicAnd* Builder::AtomicAnd(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicAnd>(Temp(to), args);
}
ir::builtins::AtomicOr* Builder::AtomicOr(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicOr>(Temp(to), args);
}
ir::builtins::AtomicXor* Builder::AtomicXor(const type::Type* to, utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicXor>(Temp(to), args);
}
ir::builtins::AtomicExchange* Builder::AtomicExchange(const type::Type* to,
                                                      utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicExchange>(Temp(to), args);
}
ir::builtins::AtomicCompareExchangeWeak* Builder::AtomicCompareExchangeWeak(
    const type::Type* to,
    utils::VectorRef<Value*> args) {
    return ir.instructions.Create<ir::builtins::AtomicCompareExchangeWeak>(Temp(to), args);
}

}  // namespace tint::ir
