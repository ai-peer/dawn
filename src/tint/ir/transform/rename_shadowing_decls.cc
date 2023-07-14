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

#include <variant>

#include "src/tint/ir/builtin_call.h"
#include "src/tint/ir/construct.h"
#include "src/tint/ir/control_instruction.h"
#include "src/tint/ir/function.h"
#include "src/tint/ir/instruction.h"
#include "src/tint/ir/loop.h"
#include "src/tint/ir/module.h"
#include "src/tint/ir/multi_in_block.h"
#include "src/tint/ir/transform/rename_shadowing_decls.h"
#include "src/tint/ir/var.h"
#include "src/tint/scope_stack.h"
#include "src/tint/switch.h"
#include "src/tint/type/matrix.h"
#include "src/tint/type/scalar.h"
#include "src/tint/type/struct.h"
#include "src/tint/type/vector.h"
#include "src/tint/utils/defer.h"
#include "src/tint/utils/hashset.h"
#include "src/tint/utils/reverse.h"
#include "src/tint/utils/string.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::transform::RenameShadowingDecls);

namespace tint::ir::transform {

/// PIMPL state for the transform, for a single function.
struct RenameShadowingDecls::State {
    /// Constructor
    /// @param i the IR module
    explicit State(Module* i) : ir(i) {}

    /// Processes the module, renaming all declarations that would prevent an identifier resolving
    /// to the correct declaration.
    void Process() {
        scopes.Push(Scope{});
        TINT_DEFER(scopes.Clear());

        RegisterModuleScopeDecls();

        // Process the module-scope variable declarations
        if (ir->root_block) {
            for (auto* inst : *ir->root_block) {
                Process(inst);
            }
        }

        // Process the functions
        for (auto* fn : ir->functions) {
            scopes.Push(Scope{});
            TINT_DEFER(scopes.Pop());
            for (auto* param : fn->Params()) {
                Process(param->Type());
                if (auto symbol = ir->NameOf(param); symbol.IsValid()) {
                    Declare(scopes.Back(), param, symbol.NameView());
                }
            }
            Process(fn->Block());
        }
    }

  private:
    /// Map of identifier to declaration.
    /// The declarations may be one of an ir::Value or type::Struct.
    using Scope = utils::Hashmap<std::string_view, CastableBase*, 8>;

    /// The IR module.
    Module* ir = nullptr;

    /// Stack of scopes
    utils::Vector<Scope, 8> scopes{};

    void RegisterModuleScopeDecls() {
        // Declare all the user types
        for (auto* ty : ir->Types()) {
            if (auto* str = ty->As<type::Struct>()) {
                auto name = str->Name().NameView();
                if (!IsBuiltinStruct(str)) {
                    Declare(scopes.Front(), const_cast<type::Struct*>(str), name);
                }
            }
        }

        // Declare all the module-scope vars
        if (ir->root_block) {
            for (auto* inst : *ir->root_block) {
                for (auto* result : inst->Results()) {
                    if (auto symbol = ir->NameOf(result)) {
                        Declare(scopes.Front(), result, symbol.NameView());
                    }
                }
            }
        }

        // Declare all the functions
        for (auto* fn : ir->functions) {
            if (auto symbol = ir->NameOf(fn); symbol.IsValid()) {
                Declare(scopes.Back(), fn, symbol.NameView());
            }
        }
    }

    void Process(Block* block) {
        for (auto* inst : *block) {
            Process(inst);
        }
    }

    void Process(Instruction* inst) {
        // Check resolving of operands
        for (auto* operand : inst->Operands()) {
            if (operand) {
                if (auto symbol = ir->NameOf(operand)) {
                    EnsureResolvesTo(symbol.NameView(), operand);
                }
                if (auto* c = operand->As<Constant>()) {
                    Process(c->Type());
                }
            }
        }

        Switch(
            inst,  //
            [&](Loop* loop) {
                scopes.Push(Scope{});
                TINT_DEFER(scopes.Pop());
                Process(loop->Initializer());
                {
                    scopes.Push(Scope{});
                    TINT_DEFER(scopes.Pop());
                    Process(loop->Body());
                    {
                        scopes.Push(Scope{});
                        TINT_DEFER(scopes.Pop());
                        Process(loop->Continuing());
                    }
                }
            },
            [&](ControlInstruction* ctrl) {
                ctrl->ForeachBlock([&](Block* block) {
                    scopes.Push(Scope{});
                    TINT_DEFER(scopes.Pop());
                    Process(block);
                });
            },
            [&](Var*) { Process(inst->Result()->Type()); },
            [&](Construct*) { Process(inst->Result()->Type()); },
            [&](BuiltinCall* call) {
                auto name = utils::ToString(call->Func());
                EnsureResolvesTo(name, nullptr);
            });

        // Register new operands and check their types can resolve
        for (auto* result : inst->Results()) {
            if (auto symbol = ir->NameOf(result); symbol.IsValid()) {
                Declare(scopes.Back(), result, symbol.NameView());
            }
        }
    }

    void Process(const type::Type* type) {
        while (type) {
            type = tint::Switch(
                type,  //
                [&](const type::Scalar* s) {
                    EnsureResolvesTo(s->FriendlyName(), nullptr);
                    return nullptr;
                },
                [&](const type::Vector* v) {
                    EnsureResolvesTo("vec" + utils::ToString(v->Width()), nullptr);
                    return v->type();
                },
                [&](const type::Matrix* m) {
                    EnsureResolvesTo(
                        "mat" + utils::ToString(m->columns()) + "x" + utils::ToString(m->rows()),
                        nullptr);
                    return m->type();
                },
                [&](const type::Pointer* p) {
                    EnsureResolvesTo(utils::ToString(p->Access()), nullptr);
                    EnsureResolvesTo(utils::ToString(p->AddressSpace()), nullptr);
                    return p->StoreType();
                },
                [&](const type::Struct* s) {
                    auto name = s->Name().NameView();
                    if (IsBuiltinStruct(s)) {
                        EnsureResolvesTo(name, nullptr);
                    } else {
                        EnsureResolvesTo(name, s);
                    }
                    return nullptr;
                });
        }
    }

    void EnsureResolvesTo(std::string_view identifier, const CastableBase* object) {
        for (auto& scope : utils::Reverse(scopes)) {
            if (auto decl = scope.Get(identifier)) {
                if (decl.value() == object) {
                    return;  // Resolved to the right thing.
                }

                // Operand is shadowed
                scope.Remove(identifier);
                Rename(decl.value(), identifier);
            }
        }
    }

    void Declare(Scope& scope, CastableBase* thing, std::string_view name) {
        auto add = scope.Add(name, thing);
        if (!add && *add.value != thing) {
            // Multiple declarations with the same name in the same scope.
            // Rename the later declaration.
            Rename(thing, name);
        }
    }

    void Rename(CastableBase* thing, std::string_view old_name) {
        Symbol new_name = ir->symbols.New(old_name);
        Switch(
            thing,  //
            [&](ir::Value* value) { ir->SetName(value, new_name); },
            [&](type::Struct* str) { str->SetName(new_name); },
            [&](Default) {
                diag::List diags;
                TINT_ICE(Transform, diags)
                    << "unhandled type for renaming: " << thing->TypeInfo().name;
            });
    }

    bool IsBuiltinStruct(const type::Struct* s) {
        // TODO(bclayton): Need to do better than this.
        return utils::HasPrefix(s->Name().NameView(), "_");
    }
};

RenameShadowingDecls::RenameShadowingDecls() = default;
RenameShadowingDecls::~RenameShadowingDecls() = default;

void RenameShadowingDecls::Run(Module* ir, const DataMap&, DataMap&) const {
    State{ir}.Process();
}

}  // namespace tint::ir::transform
