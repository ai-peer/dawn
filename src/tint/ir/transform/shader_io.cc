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
#include "src/tint/type/array.h"
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

    /// The list of shader inputs.
    utils::Vector<type::Manager::StructMemberDesc, 4> inputs;

    /// The list of shader outputs.
    utils::Vector<type::Manager::StructMemberDesc, 4> outputs;

    /// Constructor
    /// @param cfg the transform config
    /// @param mod the module
    /// @param f the function
    State(const Config& cfg, Module* mod, Function* f) : config(cfg), ir(mod), func(f) {}

    /// Destructor
    virtual ~State();

    /// Finalize the shader inputs and create any state needed for the new entry point function.
    /// @returns the list of function parameters for the new entry point
    virtual utils::Vector<FunctionParam*, 4> FinalizeInputs() = 0;

    /// Finalize the shader outputs and create any state needed for the new entry point function.
    /// @returns the return value for the new entry point
    virtual Value* FinalizeOutputs() = 0;

    /// Get the value of the input at index @p idx
    /// @param builder the IR builder for new instructions
    /// @param idx the index of the input
    /// @returns the value of the input
    virtual Value* GetInput(Builder& builder, uint32_t idx) = 0;

    /// Set the value of the output at index @p idx
    /// @param builder the IR builder for new instructions
    /// @param idx the index of the output
    /// @param value the value to set
    virtual void SetOutput(Builder& builder, uint32_t idx, Value* value) = 0;

    /// Process an entry point.
    void Process() {
        // Skip entry points with no inputs or outputs.
        if (func->Params().IsEmpty() && func->ReturnType()->Is<type::Void>()) {
            return;
        }

        // Process the parameters and return value to prepare for building a wrapper function.
        auto new_params = ProcessInputs();
        auto* new_ret_val = ProcessOutputs();

        // Rename the old function and remove its pipeline stage, as we will be
        // wrapping it with a new entry point.
        auto name = ir->NameOf(func).Name();
        auto stage = func->Stage();
        ir->SetName(func, name + "_inner");
        func->SetStage(Function::PipelineStage::kUndefined);

        // Create the entry point wrapper function.
        auto* ep = b.Function(name, new_ret_val ? new_ret_val->Type() : ty.void_());
        ep->SetStage(stage);
        ir->functions.Push(ep);
        auto wrapper = b.With(ep->Block());

        // Move the workgroup size to the wrapper if present.
        if (auto wgsize = func->WorkgroupSize()) {
            ep->SetWorkgroupSize((*wgsize)[0], (*wgsize)[1], (*wgsize)[2]);
            func->ClearWorkgroupSize();
        }

        // Call the original function, passing it the inputs and capturing its return value.
        auto inner_call_args = BuildInnerCallArgs(wrapper);
        auto* inner_result = wrapper.Call(func->ReturnType(), func, std::move(inner_call_args));
        SetOutputs(wrapper, inner_result->Result());

        // Return the new result, if needed.
        if (new_ret_val) {
            wrapper.Return(ep, new_ret_val);
        } else {
            wrapper.Return(ep);
        }
    }

    /// Process input parameters.
    /// @returns the functions parameters for the new entry point
    utils::Vector<FunctionParam*, 4> ProcessInputs() {
        for (auto* param : func->Params()) {
            if (auto* str = param->Type()->As<type::Struct>()) {
                for (auto* member : str->Members()) {
                    auto name = str->Name().Name() + "_" + member->Name().Name();
                    inputs.Push({ir->symbols.Register(name), member->Type(), member->Attributes()});
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

                auto name = ir->NameOf(param);
                inputs.Push({name, param->Type(), std::move(attributes)});
            }
        }

        return FinalizeInputs();
    }

    /// Process outputs.
    /// @returns the return value of the new entry point
    Value* ProcessOutputs() {
        if (!func->ReturnType()->Is<type::Void>()) {
            if (auto* str = func->ReturnType()->As<type::Struct>()) {
                for (auto* member : str->Members()) {
                    auto name = str->Name().Name() + "_" + member->Name().Name();
                    outputs.Push(
                        {ir->symbols.Register(name), member->Type(), member->Attributes()});
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

                outputs.Push({ir->symbols.New(), func->ReturnType(), std::move(attributes)});
            }
        }

        return FinalizeOutputs();
    }

    /// Build the argument list to call the original entry point function.
    /// @param builder the IR builder for new instructions
    /// @returns the argument list
    utils::Vector<Value*, 4> BuildInnerCallArgs(Builder& builder) {
        // Build the argument value to pass to each function parameter on the original function.
        uint32_t input_idx = 0;
        utils::Vector<Value*, 4> args;
        for (auto* param : func->Params()) {
            if (auto* str = param->Type()->As<type::Struct>()) {
                utils::Vector<Value*, 4> construct_args;
                for (uint32_t i = 0; i < str->Members().Length(); i++) {
                    construct_args.Push(GetInput(builder, input_idx++));
                }
                args.Push(builder.Construct(param->Type(), construct_args)->Result());
            } else {
                args.Push(GetInput(builder, input_idx++));
            }
        }

        return args;
    }

    /// Propagate outputs from the inner function call to their final destination.
    /// @param builder the IR builder for new instructions
    /// @param inner_result the return value from calling the original entry point function
    void SetOutputs(Builder& builder, Value* inner_result) {
        if (auto* str = inner_result->Type()->As<type::Struct>()) {
            for (auto* member : str->Members()) {
                Value* from =
                    builder.Access(member->Type(), inner_result, u32(member->Index()))->Result();
                SetOutput(builder, member->Index(), from);
            }
        } else if (!inner_result->Type()->Is<type::Void>()) {
            SetOutput(builder, 0u, inner_result);
        }
    }
};

/// PIMPL state for the parts of the transform specific to SPIR-V.
/// For SPIR-V, we split builtins and locations into two separate structures each for input and
/// output, and declare global variables for them. The wrapper entry point then loads from and
/// stores to these variables.
/// We also modify the type of the SampleMask builtin to be an array, as required by Vulkan.
struct ShaderIO::StateSpirv : State {
    /// The global variable for input builtins.
    Var* builtin_input_var = nullptr;
    /// The global variable for input locations.
    Var* location_input_var = nullptr;
    /// The global variable for output builtins.
    Var* builtin_output_var = nullptr;
    /// The global variable for output locations.
    Var* location_output_var = nullptr;
    /// The member indices for inputs.
    utils::Vector<uint32_t, 4> input_indices;
    /// The member indices for outputs.
    utils::Vector<uint32_t, 4> output_indices;

    /// Constructor
    /// @copydoc State::State
    using State::State;
    /// Destructor
    ~StateSpirv() override;

    /// Split the members listed in @p entries into two separate structures for builtins and
    /// locations, and a global variables for them. Record the new member indices in @p indices.
    /// @param builtin_var the generated global variable for builtins
    /// @param location_var the generated global variable for locations
    /// @param indices the new member indices
    /// @param entries the entries to split
    /// @param addrspace the address to use for the global variables
    /// @param access the access mode to use for the global variables
    /// @param name_suffix the suffix to add to struct and variable names
    void MakeStructs(Var** builtin_var,
                     Var** location_var,
                     utils::Vector<uint32_t, 4>* indices,
                     utils::Vector<type::Manager::StructMemberDesc, 4>& entries,
                     builtin::AddressSpace addrspace,
                     builtin::Access access,
                     const char* name_suffix) {
        // Build separate lists of builtin and location entries and record their new indices.
        uint32_t next_builtin_idx = 0;
        uint32_t next_location_idx = 0;
        utils::Vector<type::Manager::StructMemberDesc, 4> builtin_members;
        utils::Vector<type::Manager::StructMemberDesc, 4> location_members;
        for (auto io : entries) {
            if (io.attributes.builtin) {
                // SampleMask must be an array for Vulkan.
                if (io.attributes.builtin.value() == builtin::BuiltinValue::kSampleMask) {
                    io.type = ir->Types().array<u32, 1>();
                }
                builtin_members.Push(io);
                indices->Push(next_builtin_idx++);
            } else {
                location_members.Push(io);
                indices->Push(next_location_idx++);
            }
        }

        // Declare a struct and variable for builtins if needed.
        if (!builtin_members.IsEmpty()) {
            auto name = ir->NameOf(func).Name() + "_Builtin" + name_suffix + "Struct";
            auto* builtin_struct = ty.Struct(ir->symbols.New(name), std::move(builtin_members));
            *builtin_var = b.Var(ty.ptr(addrspace, builtin_struct, access));
            builtin_struct->SetStructFlag(type::kBlock);
            b.RootBlock()->Append(*builtin_var);
            ir->SetName(*builtin_var, ir->NameOf(func).Name() + "_Builtin" + name_suffix);
        }

        // Declare a struct and variable for locations if needed.
        if (!location_members.IsEmpty()) {
            auto name = ir->NameOf(func).Name() + "_Location" + name_suffix + "Struct";
            auto* location_struct = ty.Struct(ir->symbols.New(name), std::move(location_members));
            *location_var = b.Var(ty.ptr(addrspace, location_struct, access));
            location_struct->SetStructFlag(type::kBlock);
            b.RootBlock()->Append(*location_var);
            ir->SetName(*location_var, ir->NameOf(func).Name() + "_Location" + name_suffix);
        }
    }

    /// @copydoc State::FinalizeInputs
    utils::Vector<FunctionParam*, 4> FinalizeInputs() override {
        MakeStructs(&builtin_input_var, &location_input_var, &input_indices, inputs,
                    builtin::AddressSpace::kIn, builtin::Access::kRead, "Inputs");
        return utils::Empty;
    }

    /// @copydoc State::FinalizeOutputs
    Value* FinalizeOutputs() override {
        MakeStructs(&builtin_output_var, &location_output_var, &output_indices, outputs,
                    builtin::AddressSpace::kOut, builtin::Access::kWrite, "Outputs");
        return nullptr;
    }

    /// @copydoc State::GetInput
    Value* GetInput(Builder& builder, uint32_t idx) override {
        // Load the input from the global variable declared earlier.
        auto* ptr = ty.ptr(builtin::AddressSpace::kIn, inputs[idx].type, builtin::Access::kRead);
        Access* from = nullptr;
        if (inputs[idx].attributes.builtin) {
            if (inputs[idx].attributes.builtin.value() == builtin::BuiltinValue::kSampleMask) {
                // SampleMask becomes an array for SPIR-V, so load from the first element.
                from = builder.Access(ptr, builtin_input_var, u32(input_indices[idx]), 0_u);
            } else {
                from = builder.Access(ptr, builtin_input_var, u32(input_indices[idx]));
            }
        } else {
            from = builder.Access(ptr, location_input_var, u32(input_indices[idx]));
        }
        return builder.Load(from)->Result();
    }

    /// @copydoc State::SetOutput
    void SetOutput(Builder& builder, uint32_t idx, Value* value) override {
        // Store the output to the global variable declared earlier.
        auto* ptr = ty.ptr(builtin::AddressSpace::kOut, outputs[idx].type, builtin::Access::kWrite);
        Access* to = nullptr;
        if (outputs[idx].attributes.builtin) {
            if (outputs[idx].attributes.builtin.value() == builtin::BuiltinValue::kSampleMask) {
                // SampleMask becomes an array for SPIR-V, so store to the first element.
                to = builder.Access(ptr, builtin_output_var, u32(output_indices[idx]), 0_u);
            } else {
                to = builder.Access(ptr, builtin_output_var, u32(output_indices[idx]));
            }
        } else {
            to = builder.Access(ptr, location_output_var, u32(output_indices[idx]));
        }
        builder.Store(to, value);
    }
};

void ShaderIO::Run(Module* ir, const DataMap& inputs, DataMap&) const {
    auto* cfg = inputs.Get<Config>();
    TINT_ASSERT(Transform, cfg);

    // Collect all the structure members that have IO attributes, so that we can remove them later.
    utils::Vector<const type::StructMember*, 8> members_to_strip;
    for (auto* ty : ir->Types()) {
        if (auto* str = ty->As<type::Struct>()) {
            for (auto* member : str->Members()) {
                if (member->Attributes().builtin || member->Attributes().location) {
                    members_to_strip.Push(member);
                }
            }
        }
    }

    // Process each entry point function.
    for (auto* func : ir->functions) {
        if (func->Stage() != Function::PipelineStage::kUndefined) {
            switch (cfg->backend) {
                case Backend::kSpirv:
                    StateSpirv(*cfg, ir, func).Process();
                    break;
            }
        }
    }

    // Remove IO attributes from all the structure members that had them prior to this transform.
    for (auto* member : members_to_strip) {
        // TODO(jrprice): Remove the const_cast.
        const_cast<type::StructMember*>(member)->SetAttributes({});
    }
}

ShaderIO::Config::Config(Backend b) : backend(b) {}

ShaderIO::Config::~Config() = default;

ShaderIO::State::~State() = default;

ShaderIO::StateSpirv::~StateSpirv() = default;

}  // namespace tint::ir::transform
