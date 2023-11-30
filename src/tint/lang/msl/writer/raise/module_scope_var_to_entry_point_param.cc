// Copyright 2023 The Dawn & Tint Authors
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

#include "src/tint/lang/msl/writer/raise/module_scope_var_to_entry_point_param.h"

#include "src/tint/lang/core/fluent_types.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/control_instruction.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/ir/value.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/utils/containers/hashmap.h"
#include "src/tint/utils/containers/vector.h"

namespace tint::msl::writer::raise {
namespace {

/// Replace module scoped variables with entry point parameters
///
/// MSL doesn't have variables at what WGSL calls module scope. Each of those variables needs to be
/// re-written into either an entry-point parameter, or a variable created in the entry-point,
/// depending on the type. In order to simplify things we create structures for each of the
/// different address spaces in MSL (device, thread, threadgroup, constant, 'none').
///
/// Each function which uses a module scoped variable will take the respective structure as a
/// parameter and use that struct to access the member. The function calls are updated to pass
/// the needed structures down the call chain.
///
/// The structures will be initialized with the variables which are needed by the given entry
/// point. This means that some of the members in the structure maybe zero initialized if there
/// are multiple entry points which use module scoped variables from the same address space.

using namespace tint::core::fluent_types;  // NOLINT

using StructMemberDesc = core::type::Manager::StructMemberDesc;

/// Collects the objects which hold our data for each function. For a `ir::Function` these will
/// be a `FunctionParam`. For an entrypoint these will be `Value` objects.
struct FunctionData {
    core::ir::Value* privates = nullptr;
    core::ir::Value* device = nullptr;
    core::ir::Value* constant = nullptr;
    core::ir::Value* workgroup = nullptr;
    core::ir::Value* handle = nullptr;
};

bool is_entry_point(core::ir::Function* func) {
    return func->Stage() != core::ir::Function::PipelineStage::kUndefined;
}

struct State {
    /// The IR module
    core::ir::Module& ir;

    /// The builder
    core::ir::Builder b{ir};

    /// The module scoped variables
    Vector<core::ir::Var*, 5> globals{};

    /// Mapping from a block to the owing function
    Hashmap<core::ir::Block*, core::ir::Function*, 1> blk_to_function{};

    // Map from a global to the index in the respective struct
    Hashmap<core::ir::Var*, uint32_t, 5> global_to_idx{};

    // Map a function to the data objects
    Hashmap<core::ir::Function*, FunctionData, 1> function_to_data{};

    /// The structures for each type of data
    core::type::Struct* privates_struct = nullptr;
    core::type::Struct* device_struct = nullptr;
    core::type::Struct* constant_struct = nullptr;
    core::type::Struct* workgroup_struct = nullptr;
    core::type::Struct* handle_struct = nullptr;

    void Process() {
        // Record all the function blocks
        for (auto func : ir.functions) {
            blk_to_function.Add(func->Block(), func);
        }

        // Find all the globals
        for (auto* inst : *ir.root_block) {
            if (auto* v = inst->As<core::ir::Var>()) {
                globals.Push(v);
            }
        }

        BuildStructures();
        BuildFunctionData();
        SetupFunctionParams();

        Hashset<core::ir::Function*, 1> functions_to_process;

        // Replace usages
        for (auto* v : globals) {
            // Copy the usages list because we'll be modifying it as we go
            auto list = v->Result(0)->Usages();
            for (auto& usage : list) {
                auto* func = EnclosingFunctionFor(usage.instruction);
                TINT_ASSERT(func);

                ReplaceUsage(ObjectFor(func, v), usage, v);

                functions_to_process.Add(func);
            }
        }

        auto function_worklist = functions_to_process.Vector();

        // Replace calls
        Hashset<core::ir::Function*, 5> seen_functions;
        Hashset<core::ir::Function*, 1> ep_list;
        while (!function_worklist.IsEmpty()) {
            auto* func = function_worklist.Pop();

            // No call sites for entry points
            if (is_entry_point(func)) {
                continue;
            }

            if (seen_functions.Contains(func)) {
                continue;
            }
            seen_functions.Add(func);

            for (const auto& usage : func->Usages()) {
                auto* enclosing_func = EnclosingFunctionFor(usage.instruction);
                function_worklist.Push(enclosing_func);

                if (auto* call = usage.instruction->As<core::ir::UserCall>()) {
                    ExtendCallWithVars(enclosing_func, call->Target(), call);
                }
            }
        }

        // Remove the module scoped variables
        for (auto* v : globals) {
            v->Destroy();
        }
    }

    void ExtendCallWithVars(core::ir::Function* from,
                            core::ir::Function* to,
                            core::ir::UserCall* call) {
        auto& from_data = *function_to_data.GetOrZero(from);
        auto& to_data = *function_to_data.GetOrZero(to);

        if (to_data.privates != nullptr) {
            call->AppendArg(from_data.privates);
        }
        if (to_data.device != nullptr) {
            call->AppendArg(from_data.device);
        }
        if (to_data.constant != nullptr) {
            call->AppendArg(from_data.constant);
        }
        if (to_data.workgroup != nullptr) {
            call->AppendArg(from_data.workgroup);
        }
        if (to_data.handle != nullptr) {
            call->AppendArg(from_data.handle);
        }
    }

    void ReplaceUsage(core::ir::Value* object, const core::ir::Usage& usage, core::ir::Var* var) {
        auto* access = b.Access(var->Result(0)->Type(), object,
                                b.Constant(u32(global_to_idx.Get(var).value())));
        usage.instruction->Block()->InsertBefore(usage.instruction, access);
        usage.instruction->SetOperand(usage.operand_index, access->Result(0));
    }

    core::ir::Function* EnclosingFunctionFor(core::ir::Instruction* inst) {
        if (auto func = blk_to_function.Get(inst->Block()); func.has_value()) {
            return func.value();
        }

        auto* blk = inst->Block();
        while (blk) {
            // If there is no parent, then this must be a function block
            if (!blk->Parent()) {
                auto val = blk_to_function.Get(blk);
                if (val.has_value()) {
                    blk_to_function.Add(inst->Block(), val.value());
                    return val.value();
                } else {
                    TINT_UNREACHABLE();
                }
            }

            blk = blk->Parent()->Block();
        }

        return nullptr;
    }

    core::ir::Value* ObjectFor(core::ir::Function* func, core::ir::Var* v) {
        auto& data = *function_to_data.GetOrZero(func);
        auto as = v->Result(0)->Type()->As<core::type::Pointer>()->AddressSpace();
        if (as == core::AddressSpace::kWorkgroup) {
            return data.workgroup;
        }
        if (as == core::AddressSpace::kStorage) {
            return data.device;
        }
        if (as == core::AddressSpace::kUniform) {
            return data.constant;
        }
        if (as == core::AddressSpace::kHandle) {
            return data.handle;
        }
        if (as == core::AddressSpace::kPrivate) {
            return data.privates;
        }
        return nullptr;
    }

    void BuildStructures() {
        Vector<StructMemberDesc, 2> private_members{};
        Vector<StructMemberDesc, 2> device_members{};
        Vector<StructMemberDesc, 2> constant_members{};
        Vector<StructMemberDesc, 2> workgroup_members{};
        Vector<StructMemberDesc, 2> handle_members{};
        for (auto* v : globals) {
            auto as = v->Result(0)->Type()->As<core::type::Pointer>()->AddressSpace();
            if (as == core::AddressSpace::kWorkgroup) {
                StructMemberDesc member{ir.NameOf(v), v->Result(0)->Type(), {}};

                workgroup_members.Push(member);
                global_to_idx.Add(v, static_cast<uint32_t>(workgroup_members.Length() - 1));

            } else if (as == core::AddressSpace::kStorage) {
                StructMemberDesc member{ir.NameOf(v), v->Result(0)->Type(), {}};

                device_members.Push(member);
                global_to_idx.Add(v, static_cast<uint32_t>(device_members.Length() - 1));

            } else if (as == core::AddressSpace::kUniform) {
                StructMemberDesc member{ir.NameOf(v), v->Result(0)->Type(), {}};

                constant_members.Push(member);
                global_to_idx.Add(v, static_cast<uint32_t>(constant_members.Length() - 1));

            } else if (as == core::AddressSpace::kHandle) {
                StructMemberDesc member{ir.NameOf(v), v->Result(0)->Type(), {}};

                handle_members.Push(member);
                global_to_idx.Add(v, static_cast<uint32_t>(handle_members.Length() - 1));

            } else if (as == core::AddressSpace::kPrivate) {
                StructMemberDesc member{ir.NameOf(v), v->Result(0)->Type(), {}};

                private_members.Push(member);
                global_to_idx.Add(v, static_cast<uint32_t>(private_members.Length() - 1));
            }
        }

        if (!private_members.IsEmpty()) {
            privates_struct = ir.Types().Struct(ir.symbols.New("TintPrivateVars"), private_members);
        }
        if (!device_members.IsEmpty()) {
            device_struct =
                ir.Types().Struct(ir.symbols.New("TintDeviceModuleVars"), device_members);
        }
        if (!constant_members.IsEmpty()) {
            constant_struct =
                ir.Types().Struct(ir.symbols.New("TintConstantVars"), constant_members);
        }
        if (!workgroup_members.IsEmpty()) {
            workgroup_struct =
                ir.Types().Struct(ir.symbols.New("TintWorkgroupVars"), workgroup_members);
        }
        if (!handle_members.IsEmpty()) {
            handle_struct = ir.Types().Struct(ir.symbols.New("TintHandleVars"), handle_members);
        }
    }

    void BuildFunctionData() {
        Hashset<core::ir::Function*, 1> functions_to_process;
        Hashset<core::ir::Function*, 5> seen_functions;

        for (auto* v : globals) {
            for (auto& usage : v->Result(0)->Usages()) {
                auto* func = EnclosingFunctionFor(usage.instruction);
                TINT_ASSERT(func);

                if (is_entry_point(func)) {
                    CreateEntryPointParamIfNeeded(func, v);
                } else {
                    CreateFunctionParamIfNeeded(func, v);
                }
                functions_to_process.Add(func);
            }
        }

        // Propagate the param requirements up the call chain
        auto function_worklist = functions_to_process.Vector();
        while (!function_worklist.IsEmpty()) {
            auto* func = function_worklist.Pop();
            if (seen_functions.Contains(func)) {
                continue;
            }
            seen_functions.Add(func);

            // If this is an entry point nothing to propagate
            if (is_entry_point(func)) {
                continue;
            }

            for (auto& usage : func->Usages()) {
                auto* dst = EnclosingFunctionFor(usage.instruction);
                TINT_ASSERT(func);

                if (is_entry_point(dst)) {
                    CreateEntryPointParamsIfNeeded(func, dst);
                } else {
                    CreateFunctionParamsIfNeeded(func, dst);
                }
            }
        }
    }

    void SetupFunctionParams() {
        for (auto* func : function_to_data.Keys()) {
            // Entry points had the variables created when the data was setup
            if (is_entry_point(func)) {
                continue;
            }

            auto data = function_to_data.Get(func).value();
            if (data.privates != nullptr) {
                func->AddParam(data.privates->As<core::ir::FunctionParam>());
            }
            if (data.device != nullptr) {
                func->AddParam(data.device->As<core::ir::FunctionParam>());
            }
            if (data.constant != nullptr) {
                func->AddParam(data.constant->As<core::ir::FunctionParam>());
            }
            if (data.workgroup != nullptr) {
                func->AddParam(data.workgroup->As<core::ir::FunctionParam>());
            }
            if (data.handle != nullptr) {
                func->AddParam(data.handle->As<core::ir::FunctionParam>());
            }
        }
    }

    void CreateFunctionParamIfNeeded(core::ir::Function* func, core::ir::Var* v) {
        auto& data = *function_to_data.GetOrZero(func);
        auto as = v->Result(0)->Type()->As<core::type::Pointer>()->AddressSpace();
        if (as == core::AddressSpace::kWorkgroup && data.workgroup == nullptr) {
            data.workgroup = b.FunctionParam(workgroup_struct);
        }
        if (as == core::AddressSpace::kStorage && data.device == nullptr) {
            data.device = b.FunctionParam(device_struct);
        }
        if (as == core::AddressSpace::kUniform && data.constant == nullptr) {
            data.constant = b.FunctionParam(constant_struct);
        }
        if (as == core::AddressSpace::kHandle && data.handle == nullptr) {
            data.handle = b.FunctionParam(handle_struct);
        }
        if (as == core::AddressSpace::kPrivate && data.privates == nullptr) {
            data.privates =
                b.FunctionParam(ir.Types().ptr(core::AddressSpace::kFunction, privates_struct));
        }
    }

    // Copies the needed params from `src` to `dst`
    void CreateFunctionParamsIfNeeded(core::ir::Function* src, core::ir::Function* dst) {
        auto& src_data = *function_to_data.GetOrZero(src);
        auto& dst_data = *function_to_data.GetOrZero(dst);

        if (src_data.workgroup && !dst_data.workgroup) {
            dst_data.workgroup = b.FunctionParam(workgroup_struct);
        }
        if (src_data.device && !dst_data.device) {
            dst_data.device = b.FunctionParam(device_struct);
        }

        if (src_data.constant && !dst_data.constant) {
            dst_data.constant = b.FunctionParam(constant_struct);
        }
        if (src_data.handle && !dst_data.handle) {
            dst_data.handle = b.FunctionParam(handle_struct);
        }
        if (src_data.privates && !dst_data.privates) {
            dst_data.privates =
                b.FunctionParam(ir.Types().ptr(core::AddressSpace::kFunction, privates_struct));
        }
    }

    // TODO(dsinclair): These are all wrong ...
    // The entrypoint creation needs to be smarter. The types aren't right in all cases and the
    // construct calls need to pass in any provided entry point data (like buffers and textures) as
    // are used by the entry point. The entry point needs to be updated to accept the given buffers
    // and textures in the signature.

    void CreateEntryPointParamIfNeeded(core::ir::Function* func, core::ir::Var* module_var) {
        auto& data = *function_to_data.GetOrZero(func);
        auto as = module_var->Result(0)->Type()->As<core::type::Pointer>()->AddressSpace();

        if (as == core::AddressSpace::kWorkgroup && data.workgroup == nullptr) {
            auto* v = b.Construct(workgroup_struct);
            func->Block()->Prepend(v);
            data.workgroup = v->Result(0);
        }
        if (as == core::AddressSpace::kStorage && data.device == nullptr) {
            auto* v = b.Construct(device_struct);
            func->Block()->Prepend(v);
            data.device = v->Result(0);
        }

        if (as == core::AddressSpace::kUniform && data.constant == nullptr) {
            auto* v = b.Construct(constant_struct);
            func->Block()->Prepend(v);
            data.constant = v->Result(0);
        }
        if (as == core::AddressSpace::kHandle && data.handle == nullptr) {
            auto* v = b.Construct(handle_struct);
            func->Block()->Prepend(v);
            data.handle = v->Result(0);
        }
        if (as == core::AddressSpace::kPrivate && data.privates == nullptr) {
            auto* v = b.Construct(privates_struct);
            func->Block()->Prepend(v);
            data.privates = v->Result(0);
        }
    }
    // Creates the needed params based on `src` in `ep`
    void CreateEntryPointParamsIfNeeded(core::ir::Function* src, core::ir::Function* ep) {
        auto& src_data = *function_to_data.GetOrZero(src);
        auto& ep_data = *function_to_data.GetOrZero(ep);

        if (src_data.workgroup && !ep_data.workgroup) {
            auto* v = b.Construct(workgroup_struct);
            ep->Block()->Prepend(v);
            ep_data.workgroup = v->Result(0);
        }
        if (src_data.device && !ep_data.device) {
            auto* v = b.Construct(device_struct);
            ep->Block()->Prepend(v);
            ep_data.device = v->Result(0);
        }

        if (src_data.constant && !ep_data.constant) {
            auto* v = b.Construct(constant_struct);
            ep->Block()->Prepend(v);
            ep_data.constant = v->Result(0);
        }
        if (src_data.handle && !ep_data.handle) {
            auto* v = b.Construct(handle_struct);
            ep->Block()->Prepend(v);
            ep_data.handle = v->Result(0);
        }
        if (src_data.privates && !ep_data.privates) {
            auto* v = b.Construct(privates_struct);
            ep->Block()->Prepend(v);
            ep_data.privates = v->Result(0);
        }
    }
};

}  // namespace

Result<SuccessType> ModuleScopeVarToEntryPointParam(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "ModuleScopeVarToEntryPointParam transform");
    if (!result) {
        return result.Failure();
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::msl::writer::raise
