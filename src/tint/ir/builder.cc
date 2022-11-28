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
#include "src/tint/program.h"

namespace tint::ir {

Builder::Builder(const Program* prog) : ir(prog) {}

Builder::Builder(Module&& mod) : ir(std::move(mod)) {}

Builder::~Builder() = default;

Block* Builder::CreateBlock() {
    return ir.flow_nodes.Create<Block>();
}

Terminator* Builder::CreateTerminator() {
    return ir.flow_nodes.Create<Terminator>();
}

Function* Builder::CreateFunction(const ast::Function* ast_func) {
    auto* ir_func = ir.flow_nodes.Create<Function>(ast_func);
    ir_func->start_target = CreateBlock();
    ir_func->end_target = CreateTerminator();

    // Function is always branching into the start target
    ir_func->start_target->inbound_branches.Push(ir_func);

    return ir_func;
}

If* Builder::CreateIf(const ast::Statement* stmt) {
    auto* ir_if = ir.flow_nodes.Create<If>(stmt);
    ir_if->true_target = CreateBlock();
    ir_if->false_target = CreateBlock();
    ir_if->merge_target = CreateBlock();

    // An if always branches to both the true and false block.
    ir_if->true_target->inbound_branches.Push(ir_if);
    ir_if->false_target->inbound_branches.Push(ir_if);

    return ir_if;
}

Loop* Builder::CreateLoop(const ast::Statement* stmt) {
    auto* ir_loop = ir.flow_nodes.Create<Loop>(stmt);
    ir_loop->start_target = CreateBlock();
    ir_loop->continuing_target = CreateBlock();
    ir_loop->merge_target = CreateBlock();

    // A loop always branches to the start block.
    ir_loop->start_target->inbound_branches.Push(ir_loop);

    return ir_loop;
}

Switch* Builder::CreateSwitch(const ast::SwitchStatement* stmt) {
    auto* ir_switch = ir.flow_nodes.Create<Switch>(stmt);
    ir_switch->merge_target = CreateBlock();
    return ir_switch;
}

Block* Builder::CreateCase(Switch* s, const utils::VectorRef<const ast::CaseSelector*> selectors) {
    s->cases.Push(Switch::Case{selectors, CreateBlock()});

    Block* b = s->cases.Back().start_target;
    // Switch branches into the case block
    b->inbound_branches.Push(s);
    return b;
}

void Builder::Branch(Block* from, FlowNode* to) {
    TINT_ASSERT(IR, from);
    TINT_ASSERT(IR, to);
    from->branch_target = to;
    to->inbound_branches.Push(from);
}

Value::Id Builder::AllocateValue() {
    return next_value_id++;
}

Instruction Builder::CreateInstruction(Instruction::Kind kind, Value lhs, Value rhs) {
    return Instruction(kind, Value(AllocateValue()), lhs, rhs);
}

Instruction Builder::And(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kAnd, lhs, rhs);
}

Instruction Builder::Or(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kOr, lhs, rhs);
}

Instruction Builder::Xor(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kXor, lhs, rhs);
}

Instruction Builder::LogicalAnd(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kLogicalAnd, lhs, rhs);
}

Instruction Builder::LogicalOr(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kLogicalOr, lhs, rhs);
}

Instruction Builder::Equal(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kEqual, lhs, rhs);
}

Instruction Builder::NotEqual(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kNotEqual, lhs, rhs);
}

Instruction Builder::LessThan(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kLessThan, lhs, rhs);
}

Instruction Builder::GreaterThan(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kGreaterThan, lhs, rhs);
}

Instruction Builder::LessThanEqual(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kLessThanEqual, lhs, rhs);
}

Instruction Builder::GreaterThanEqual(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kGreaterThanEqual, lhs, rhs);
}

Instruction Builder::ShiftLeft(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kShiftLeft, lhs, rhs);
}

Instruction Builder::ShiftRight(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kShiftRight, lhs, rhs);
}

Instruction Builder::Add(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kAdd, lhs, rhs);
}

Instruction Builder::Subtract(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kSubtract, lhs, rhs);
}

Instruction Builder::Multiply(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kMultiply, lhs, rhs);
}

Instruction Builder::Divide(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kDivide, lhs, rhs);
}

Instruction Builder::Modulo(Value lhs, Value rhs) {
    return CreateInstruction(Instruction::Kind::kModulo, lhs, rhs);
}

}  // namespace tint::ir
