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

#include "src/tint/ir/validate.h"

#include <utility>

#include "src/tint/ir/function.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/loop.h"
#include "src/tint/ir/switch.h"
#include "src/tint/ir/var.h"
#include "src/tint/switch.h"
#include "src/tint/type/pointer.h"

namespace tint::ir {
namespace {

class Validator {
  public:
    explicit Validator(const Module& mod) : mod_(mod) {}

    ~Validator() {}

    utils::Result<bool, diag::List> IsValid() {
        CheckRootBlock(mod_.root_block);

        for (const auto* func : mod_.functions) {
            CheckFunction(func);
        }

        if (diagnostics_.contains_errors()) {
            return std::move(diagnostics_);
        }
        return true;
    }

  private:
    const Module& mod_;
    diag::List diagnostics_;

    void add_error(const std::string& err) { diagnostics_.add_error(tint::diag::System::IR, err); }

    std::string Name(const Value* v) { return mod_.NameOf(v).Name(); }

    void CheckRootBlock(const Block* blk) {
        if (!blk) {
            return;
        }

        for (const auto* inst : *blk) {
            auto* var = inst->As<ir::Var>();
            if (!var) {
                add_error(std::string("root block: invalid instruction: ") + inst->TypeInfo().name);
                continue;
            }
            if (!var->Type()->Is<type::Pointer>()) {
                add_error(std::string("root block: 'var' ") + Name(var) +
                          "type is not a pointer: " + var->Type()->TypeInfo().name);
            }
        }
    }

    void CheckFunction(const Function* func) {
        for (const auto* param : func->Params()) {
            if (param == nullptr) {
                add_error("null parameter in function " + Name(func));
                continue;
            }

            utils::Vector<const Instruction*, 4> v(param->Usage());
            if (!v.Any([func](const Value* inst) {
                    return inst->Is<ir::Function>() && inst->As<ir::Function>() == func;
                })) {
                add_error("function " + Name(func) + " param does not have function in usage");
            }
        }

        if (!func->StartTarget()) {
            add_error("function " + Name(func) + " start target is null");
        }
        CheckBlock(func->StartTarget());
    }

    void CheckBlock(const Block* blk) {
        if (!blk->HasBranchTarget()) {
            add_error("block does not end in a branch");
        }

        for (const auto* inst : *blk) {
            if (!inst) {
                add_error("block with nullptr instruction");
                continue;
            }
            CheckInstruction(inst);
        }
    }

    void CheckInstruction(const Instruction* inst) {
        tint::Switch(
            inst,  //
            [&](const ir::If* if_) {
                if (if_->Condition() == nullptr) {
                    add_error("if instruction with a null condition");
                }
            },
            [&](Default) {
                add_error(std::string("missing validation of: ") + inst->TypeInfo().name);
            });
    }
};

}  // namespace

utils::Result<bool, std::string> Validate(const Module& mod) {
    Validator v(mod);
    auto r = v.IsValid();
    if (!r) {
        return r.Failure().str();
    }
    return true;
}

}  // namespace tint::ir
