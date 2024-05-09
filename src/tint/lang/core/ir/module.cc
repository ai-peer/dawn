// Copyright 2022 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/core/ir/module.h"

#include <limits>
#include <utility>

#include "src/tint/lang/core/ir/control_instruction.h"
#include "src/tint/lang/core/ir/user_call.h"
#include "src/tint/utils/ice/ice.h"

namespace tint::core::ir {

Module::Module() : root_block(blocks.Create<ir::Block>()) {}

Module::Module(Module&&) = default;

Module::~Module() = default;

Module& Module::operator=(Module&&) = default;

Symbol Module::NameOf(const Instruction* inst) const {
    if (inst->Results().Length() != 1) {
        return Symbol{};
    }
    return NameOf(inst->Result(0));
}

Symbol Module::NameOf(const Value* value) const {
    return value_to_name_.GetOr(value, Symbol{});
}

void Module::SetName(Instruction* inst, std::string_view name) {
    TINT_ASSERT(inst->Results().Length() == 1);
    return SetName(inst->Result(0), name);
}

void Module::SetName(Value* value, std::string_view name) {
    TINT_ASSERT(!name.empty());
    value_to_name_.Replace(value, symbols.Register(name));
}

void Module::SetName(Value* value, Symbol name) {
    TINT_ASSERT(name.IsValid());
    value_to_name_.Replace(value, name);
}

void Module::ClearName(Value* value) {
    value_to_name_.Remove(value);
}

namespace {

/// Helper to sort a module's function in dependency order.
struct FunctionSorter {
    const Module* mod;
    Vector<const Function*, 16> ordered_functions{};
    Hashset<const Function*, 16> visited{};

    /// Visit a function and look for dependencies, if the function hasn't already been visited.
    /// @param func the function to visit
    void Visit(const Function* func) {
        if (visited.Add(func)) {
            Visit(func->Block());
            ordered_functions.Push(func);
        }
    }

    /// Visit a block and look for dependencies.
    /// @param block the block to visit
    void Visit(const Block* block) {
        for (auto* inst : *block) {
            if (auto* control = inst->As<ControlInstruction>()) {
                // Recurse into child blocks.
                control->ForeachBlock([&](const Block* b) { Visit(b); });
            } else if (auto* call = inst->As<UserCall>()) {
                // Visit the function that is being called.
                Visit(call->Target());
            }
        }
    }
};

}  // namespace

Vector<const Function*, 16> Module::DependencyOrderedFunctions() const {
    FunctionSorter sorter{this};
    for (auto& func : functions) {
        sorter.Visit(func);
    }
    return std::move(sorter.ordered_functions);
}

}  // namespace tint::core::ir
