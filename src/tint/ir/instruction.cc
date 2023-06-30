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

#include "src/tint/ir/instruction.h"

#include "src/tint/debug.h"
#include "src/tint/ir/block.h"
#include "src/tint/ir/continue.h"
#include "src/tint/ir/exit_if.h"
#include "src/tint/ir/exit_loop.h"
#include "src/tint/ir/exit_switch.h"
#include "src/tint/ir/next_iteration.h"
#include "src/tint/ir/return.h"
#include "src/tint/ir/unreachable.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::Instruction);

namespace tint::ir {

Instruction::Instruction() = default;

Instruction::~Instruction() = default;

void Instruction::Destroy() {
    TINT_ASSERT(IR, Alive());
    if (Block()) {
        Remove();
    }
    for (auto* result : Results()) {
        result->SetSource(nullptr);
        result->Destroy();
    }
    flags_.Add(Flag::kDead);
}

void Instruction::InsertBefore(Instruction* before) {
    TINT_ASSERT_OR_RETURN(IR, before);
    TINT_ASSERT_OR_RETURN(IR, before->Block() != nullptr);
    before->Block()->InsertBefore(before, this);
}

void Instruction::InsertAfter(Instruction* after) {
    TINT_ASSERT_OR_RETURN(IR, after);
    TINT_ASSERT_OR_RETURN(IR, after->Block() != nullptr);
    after->Block()->InsertAfter(after, this);
}

void Instruction::ReplaceWith(Instruction* replacement) {
    TINT_ASSERT_OR_RETURN(IR, replacement);
    TINT_ASSERT_OR_RETURN(IR, Block() != nullptr);
    Block()->Replace(this, replacement);
}

void Instruction::Remove() {
    TINT_ASSERT_OR_RETURN(IR, Block() != nullptr);
    Block()->Remove(this);
}

std::string_view Instruction::FriendlyNameOf(const tint::utils::TypeInfo& ti) {
    if (&ti == &utils::TypeInfo::Of<ir::Terminator>()) {
        return "any terminator";
    }
    if (&ti == &utils::TypeInfo::Of<ir::Exit>()) {
        return "any exit";
    }
    if (&ti == &utils::TypeInfo::Of<ir::Continue>()) {
        return "continue";
    }
    if (&ti == &utils::TypeInfo::Of<ir::ExitIf>()) {
        return "exit_if";
    }
    if (&ti == &utils::TypeInfo::Of<ir::ExitLoop>()) {
        return "exit_loop";
    }
    if (&ti == &utils::TypeInfo::Of<ir::ExitSwitch>()) {
        return "exit_switch";
    }
    if (&ti == &utils::TypeInfo::Of<ir::NextIteration>()) {
        return "next_iteration";
    }
    if (&ti == &utils::TypeInfo::Of<ir::Return>()) {
        return "return";
    }
    if (&ti == &utils::TypeInfo::Of<ir::Unreachable>()) {
        return "unreachable";
    }
    return ti.name;
}

}  // namespace tint::ir
