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

#include "src/tint/ir/transform/shader_io.h"

#include <utility>

#include "src/tint/ir/builder.h"
#include "src/tint/ir/module.h"
#include "src/tint/type/struct.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::transform::ShaderIO);
TINT_INSTANTIATE_TYPEINFO(tint::ir::transform::ShaderIO::Config);

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

namespace tint::ir::transform {

namespace {

builtin::BuiltinValue FunctionParamBuiltin(enum FunctionParam::Builtin builtin) {
    switch (builtin) {
        case FunctionParam::Builtin::kVertexIndex:
            return builtin::BuiltinValue::kVertexIndex;
        case FunctionParam::Builtin::kInstanceIndex:
            return builtin::BuiltinValue::kInstanceIndex;
        case FunctionParam::Builtin::kPosition:
            return builtin::BuiltinValue::kPosition;
        case FunctionParam::Builtin::kFrontFacing:
            return builtin::BuiltinValue::kFrontFacing;
        case FunctionParam::Builtin::kLocalInvocationId:
            return builtin::BuiltinValue::kLocalInvocationId;
        case FunctionParam::Builtin::kLocalInvocationIndex:
            return builtin::BuiltinValue::kLocalInvocationIndex;
        case FunctionParam::Builtin::kGlobalInvocationId:
            return builtin::BuiltinValue::kGlobalInvocationId;
        case FunctionParam::Builtin::kWorkgroupId:
            return builtin::BuiltinValue::kWorkgroupId;
        case FunctionParam::Builtin::kNumWorkgroups:
            return builtin::BuiltinValue::kNumWorkgroups;
        case FunctionParam::Builtin::kSampleIndex:
            return builtin::BuiltinValue::kSampleIndex;
        case FunctionParam::Builtin::kSampleMask:
            return builtin::BuiltinValue::kSampleMask;
    }
}

builtin::BuiltinValue ReturnBuiltin(enum Function::ReturnBuiltin builtin) {
    switch (builtin) {
        case Function::ReturnBuiltin::kPosition:
            return builtin::BuiltinValue::kPosition;
        case Function::ReturnBuiltin::kFragDepth:
            return builtin::BuiltinValue::kFragDepth;
        case Function::ReturnBuiltin::kSampleMask:
            return builtin::BuiltinValue::kSampleMask;
    }
}

}  // namespace

ShaderIO::ShaderIO() = default;

ShaderIO::~ShaderIO() = default;

/// PIMPL state for the transform, for a single entry point function.
struct ShaderIO::State {
    /// The configuration data.
    const Config& config;
    /// The IR module.
    Module* ir = nullptr;
    /// The IR builder.
    Builder b{*ir};
    /// The type manager.
    type::Manager& ty{ir->Types()};

    /// The list of struct members used as shader inputs.
    utils::Vector<const type::StructMember*, 4> input_struct_members;

    /// The list of struct members used as shader outputs.
    utils::Vector<const type::StructMember*, 4> output_struct_members;

    /// Constructor
    /// @param mod the module
    State(const Config& cfg, Module* mod) : config(cfg), ir(mod) {}

    /// Process an entry point.
    /// @param func the entry point to process.
    void Process(Function* func) {
        // Skip entry points with no inputs or outputs.
        if (func->Params().IsEmpty() && func->ReturnType()->Is<type::Void>()) {
            return;
        }

        // Process input parameters.
        for (auto* param : func->Params()) {
            // TODO: if struct, process members
            type::StructMemberAttributes attributes;
            if (auto loc = param->Location()) {
                attributes.location = loc->value;
                if (loc->interpolation) {
                    attributes.interpolation = *loc->interpolation;
                }
            } else if (auto builtin = param->Builtin()) {
                attributes.builtin = FunctionParamBuiltin(*builtin);
            }
            attributes.invariant = param->Invariant();
            AddInput(param->Type(), std::move(attributes));
            // TODO: rewrite param without attributes on inner function
        }

        // TODO: return values
        if (!func->ReturnType()->Is<type::Void>()) {
            // TODO: if struct,F process members
            type::StructMemberAttributes attributes;
            if (auto loc = func->ReturnLocation()) {
                attributes.location = loc->value;
                if (loc->interpolation) {
                    attributes.interpolation = *loc->interpolation;
                }
            } else if (auto builtin = func->ReturnBuiltin()) {
                attributes.builtin = ReturnBuiltin(*builtin);
            }
            attributes.invariant = func->ReturnInvariant();
            AddOutput(func->ReturnType(), std::move(attributes));

            // TODO: remove attributes on inner function
        }

        // Rename the old function, as we will be wrapping it with a new entry point.
        auto name = ir->NameOf(func).Name();
        ir->SetName(func, name + "_inner");

        // Create the entry point wrapper function.
        auto* ep = b.Function(name, ty.void_());
        ep->SetStage(func->Stage());
        func->SetStage(Function::PipelineStage::kUndefined);
        ir->functions.Push(ep);
        auto wrapper = b.With(ep->StartTarget());

        // TODO: load inputs
        utils::Vector<Value*, 4> inner_call_args;
        if (!input_struct_members.IsEmpty()) {
            auto* input_struct = MakeStruct(std::move(input_struct_members));

            Var* input_var = nullptr;
            switch (config.backend) {
                case Backend::kSpirv:
                    input_struct->SetStructFlag(type::kBlock);
                    input_var = b.Var(
                        ty.ptr(builtin::AddressSpace::kIn, input_struct, builtin::Access::kRead));
                    b.RootBlock()->Append(input_var);
                    break;
            }
            for (auto* member : input_struct->Members()) {
                auto* from = wrapper.Access(ty.ptr(input_var->Type()->AddressSpace(),
                                                   member->Type(), builtin::Access::kRead),
                                            input_var, u32(member->Index()));
                auto* input = wrapper.Load(from);
                // TODO: Might need to construct if original was a struct
                inner_call_args.Push(input);
            }
        }

        // Call the old function and capture its return value.
        auto* inner_result = wrapper.Call(func->ReturnType(), func, std::move(inner_call_args));

        // TODO: set outputs
        if (!output_struct_members.IsEmpty()) {
            auto* output_struct = MakeStruct(std::move(output_struct_members));

            Var* output_var = nullptr;
            switch (config.backend) {
                case Backend::kSpirv:
                    output_struct->SetStructFlag(type::kBlock);
                    output_var = b.Var(ty.ptr(builtin::AddressSpace::kOut, output_struct,
                                              builtin::Access::kWrite));
                    b.RootBlock()->Append(output_var);
                    break;
            }
            for (auto* member : output_struct->Members()) {
                Value* from = inner_result;
                if (from->Type()->Is<type::Struct>()) {
                    // TODO: index might not be the same
                    from = wrapper.Access(member->Type(), from, u32(member->Index()));
                }
                auto* to = wrapper.Access(ty.ptr(output_var->Type()->AddressSpace(), member->Type(),
                                                 builtin::Access::kWrite),
                                          output_var, u32(member->Index()));
                wrapper.Store(to, from);
            }
        }

        // TODO: return value if not SPIR-V
        wrapper.Return(ep);
    }

    /// TODO
    void AddInput(const type::Type* type, type::StructMemberAttributes attributes) {
        PushStructMember(input_struct_members, type, std::move(attributes));
    }

    /// TODO
    void AddOutput(const type::Type* type, type::StructMemberAttributes attributes) {
        PushStructMember(output_struct_members, type, std::move(attributes));
    }

    /// Create and append struct member to a list, inferring its offset, alignment, and size.
    /// @param member_list the list of structure members to append to
    /// @param type the member type
    /// @param attributes the member attributes
    void PushStructMember(utils::Vector<const type::StructMember*, 4>& member_list,
                          const type::Type* type,
                          type::StructMemberAttributes&& attributes) {
        uint32_t offset = 0;
        if (member_list.Length() > 0) {
            offset = member_list.Back()->Offset() + member_list.Back()->Size();
            offset = utils::RoundUp(type->Align(), offset);
        }
        member_list.Push(ty.Get<type::StructMember>(ir->symbols.New(), type, member_list.Length(),
                                                    offset, type->Align(), type->Size(),
                                                    std::move(attributes)));
    }

    /// Create a struct from a list of members, inferring the size and alignment.
    /// @param members the structure members
    /// @returns the structure
    type::Struct* MakeStruct(utils::VectorRef<const type::StructMember*> members) {
        uint32_t align = 0;
        uint32_t size = 0;
        for (auto* member : members) {
            align = std::max(align, member->Align());
            size = member->Offset() + member->Size();
        }
        return ty.Get<type::Struct>(ir->symbols.New(), std::move(members), align,
                                    utils::RoundUp(align, size), size);
    }
};

void ShaderIO::Run(Module* ir, const DataMap& inputs, DataMap&) const {
    auto* cfg = inputs.Get<Config>();
    TINT_ASSERT(Transform, cfg);

    // Process each entry point function.
    for (auto* func : ir->functions) {
        if (func->Stage() != Function::PipelineStage::kUndefined) {
            State state(*cfg, ir);
            state.Process(func);
        }
    }
}

ShaderIO::Config::Config(Backend b) : backend(b) {}

ShaderIO::Config::~Config() = default;

}  // namespace tint::ir::transform
