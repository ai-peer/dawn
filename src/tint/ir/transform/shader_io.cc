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

#include <functional>
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

    /// The original entry point function.
    Function* func = nullptr;

    /// The list of struct members used as shader inputs.
    utils::Vector<const type::StructMember*, 4> input_struct_members;

    /// The list of struct members used as shader outputs.
    utils::Vector<const type::StructMember*, 4> output_struct_members;

    /// Constructor
    /// @param cfg the transform config
    /// @param mod the module
    /// @param f the function
    State(const Config& cfg, Module* mod, Function* f) : config(cfg), ir(mod), func(f) {}

    /// Process an entry point.
    void Process() {
        // Skip entry points with no inputs or outputs.
        if (func->Params().IsEmpty() && func->ReturnType()->Is<type::Void>()) {
            return;
        }

        // Process the parameters and return value to prepare for building a wrapper function.
        ProcessInputs();
        ProcessOutputs();

        // Rename the old function and remove its pipeline stage, as we will be wrapping it with a
        // new entry point.
        auto name = ir->NameOf(func).Name();
        auto stage = func->Stage();
        ir->SetName(func, name + "_inner");
        func->SetStage(Function::PipelineStage::kUndefined);

        // Create the entry point wrapper function.
        // TODO(jrprice): MSL and HLSL need a non-void return type here.
        auto* ep = b.Function(name, ty.void_());
        ep->SetStage(stage);
        ir->functions.Push(ep);
        auto wrapper = b.With(ep->StartTarget());

        // Call the original function, passing it the inputs and capturing its return value.
        auto inner_call_args = BuildInnerCallArgs(wrapper);
        auto* inner_result = wrapper.Call(func->ReturnType(), func, std::move(inner_call_args));
        SetOutputs(wrapper, inner_result->Result());

        // TODO: return value if not SPIR-V
        wrapper.Return(ep);
    }

    /// Process input parameters.
    void ProcessInputs() {
        for (auto* param : func->Params()) {
            if (auto* str = param->Type()->As<type::Struct>()) {
                for (auto* member : str->Members()) {
                    PushStructMember(input_struct_members, member->Type(), member->Attributes());
                    // TODO: Can we do this without having to const_cast?
                    const_cast<type::StructMember*>(member)->SetAttributes({});
                }
            } else {
                // Pull out the IO attributes and remove them from the parameter.
                type::StructMemberAttributes attributes;
                if (auto loc = param->Location()) {
                    attributes.location = loc->value;
                    if (loc->interpolation) {
                        attributes.interpolation = *loc->interpolation;
                    }
                    param->Location() = {};
                } else if (auto builtin = param->Builtin()) {
                    attributes.builtin = FunctionParamBuiltin(*builtin);
                    param->Builtin() = {};
                }
                attributes.invariant = param->Invariant();
                param->SetInvariant(false);

                PushStructMember(input_struct_members, param->Type(), std::move(attributes));
            }
        }
    }

    /// Process outputs.
    void ProcessOutputs() {
        if (!func->ReturnType()->Is<type::Void>()) {
            if (auto* str = func->ReturnType()->As<type::Struct>()) {
                for (auto* member : str->Members()) {
                    PushStructMember(output_struct_members, member->Type(), member->Attributes());
                    // TODO: Can we do this without having to const_cast?
                    const_cast<type::StructMember*>(member)->SetAttributes({});
                }
            } else {
                // Pull out the IO attributes and remove them from the original function.
                type::StructMemberAttributes attributes;
                if (auto loc = func->ReturnLocation()) {
                    attributes.location = loc->value;
                    func->ReturnLocation() = {};
                } else if (auto builtin = func->ReturnBuiltin()) {
                    attributes.builtin = ReturnBuiltin(*builtin);
                    func->ReturnBuiltin() = {};
                }
                attributes.invariant = func->ReturnInvariant();
                func->SetReturnInvariant(false);

                PushStructMember(output_struct_members, func->ReturnType(), std::move(attributes));
            }
        }
    }

    /// Create and append struct member to a list, inferring its offset, alignment, and size.
    /// @param member_list the list of structure members to append to
    /// @param type the member type
    /// @param attributes the member attributes
    void PushStructMember(utils::Vector<const type::StructMember*, 4>& member_list,
                          const type::Type* type,
                          type::StructMemberAttributes attributes) {
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

    /// Build the argument list to call in original entry point function.
    /// @param builder the IR builder for new instructions
    /// @returns the argument list
    utils::Vector<Value*, 4> BuildInnerCallArgs(Builder& builder) {
        if (input_struct_members.IsEmpty()) {
            return utils::Empty;
        }

        // Set up any state needed to handle inputs for the target backend, and make a functor that
        // will produce the value that corresponds to a given input.
        std::function<Value*(const type::Type*, uint32_t)> get_input;
        switch (config.backend) {
            case Backend::kSpirv: {
                // TODO: needs separate structs for builtin vs location
                // Declare a global variable in the `in` address space whose type is a struct that
                // contains every input value.
                auto* input_struct = MakeStruct(std::move(input_struct_members));
                auto* input_var =
                    b.Var(ty.ptr(builtin::AddressSpace::kIn, input_struct, builtin::Access::kRead));
                input_struct->SetStructFlag(type::kBlock);
                b.RootBlock()->Append(input_var);

                get_input = [&, input_var](const type::Type* type, uint32_t idx) {
                    auto* from = builder.Access(
                        ty.ptr(builtin::AddressSpace::kIn, type, builtin::Access::kRead),
                        input_var->Result(), u32(idx));
                    return builder.Load(from)->Result();
                };
                break;
            }
        }

        // Build the argument value to pass to each function parameter on the original function.
        uint32_t input_idx = 0;
        utils::Vector<Value*, 4> args;
        for (auto* param : func->Params()) {
            if (auto* str = param->Type()->As<type::Struct>()) {
                utils::Vector<Value*, 4> construct_args;
                for (auto* member : str->Members()) {
                    construct_args.Push(get_input(member->Type(), input_idx++));
                }
                args.Push(builder.Construct(param->Type(), construct_args)->Result());
            } else {
                args.Push(get_input(param->Type(), input_idx++));
            }
        }

        return args;
    }

    /// Propagate outputs from the inner function call to their final destination.
    /// @param builder the IR builder for new instructions
    /// @param inner_result the return value from calling the original entry point function
    void SetOutputs(Builder& builder, Value* inner_result) {
        if (output_struct_members.IsEmpty()) {
            return;
        }

        // Set up any state needed to handle outputs for the target backend.
        Var* output_var = nullptr;
        auto* output_struct = MakeStruct(std::move(output_struct_members));
        switch (config.backend) {
            case Backend::kSpirv: {
                // TODO: needs separate structs for builtin vs location
                output_var = b.Var(
                    ty.ptr(builtin::AddressSpace::kOut, output_struct, builtin::Access::kWrite));
                output_struct->SetStructFlag(type::kBlock);
                b.RootBlock()->Append(output_var);
                break;
            }
        }

        for (auto* member : output_struct->Members()) {
            Value* from = inner_result;
            if (from->Type()->Is<type::Struct>()) {
                // TODO: index might not be the same
                from = builder.Access(member->Type(), from, u32(member->Index()))->Result();
            }
            auto* to = builder.Access(
                ty.ptr(output_var->Result()->Type()->As<type::Pointer>()->AddressSpace(),
                       member->Type(), builtin::Access::kWrite),
                output_var, u32(member->Index()));
            builder.Store(to, from);
        }
    }
};

void ShaderIO::Run(Module* ir, const DataMap& inputs, DataMap&) const {
    auto* cfg = inputs.Get<Config>();
    TINT_ASSERT(Transform, cfg);

    // Process each entry point function.
    for (auto* func : ir->functions) {
        if (func->Stage() != Function::PipelineStage::kUndefined) {
            State state(*cfg, ir, func);
            state.Process();
        }
    }
}

ShaderIO::Config::Config(Backend b) : backend(b) {}

ShaderIO::Config::~Config() = default;

}  // namespace tint::ir::transform
