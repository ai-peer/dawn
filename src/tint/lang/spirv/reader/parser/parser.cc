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

#include "src/tint/lang/spirv/reader/parser/parser.h"

#include <memory>
#include <utility>
#include <vector>

#include "source/opt/build_module.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/spirv/validate/validate.h"

namespace tint::spirv::reader {

namespace {

/// The SPIR-V environment that we validate against.
constexpr auto kTargetEnv = SPV_ENV_VULKAN_1_1;

/// PIMPL class for SPIR-V parser.
class Parser {
  public:
    /// @param spirv the SPIR-V binary data
    /// @returns the generated SPIR-V IR module on success, or failure
    Result<core::ir::Module> Run(Slice<const uint32_t> spirv) {
        // Validate the incoming SPIR-V binary.
        auto result = validate::Validate(spirv, kTargetEnv);
        if (!result) {
            return result.Failure();
        }

        // Build the SPIR-V tools internal representation of the SPIR-V module.
        spvtools::Context context(kTargetEnv);
        spirv_context_ =
            spvtools::BuildModule(kTargetEnv, context.CContext()->consumer, spirv.data, spirv.len);
        if (!spirv_context_) {
            return Failure("failed to build the internal representation of the module");
        }

        // TODO(crbug.com/tint/1907): Emit module-scope variables.

        EmitFunctions();

        EmitEntryPoints();

        // TODO(crbug.com/tint/1907): Handle annotation instructions.
        // TODO(crbug.com/tint/1907): Handle names.

        return std::move(ir_);
    }

    /// @param type a SPIR-V type object
    /// @returns a Tint type object
    const core::type::Type* Type(const spvtools::opt::analysis::Type* type) {
        switch (type->kind()) {
            case spvtools::opt::analysis::Type::kVoid:
                return ty_.void_();
            case spvtools::opt::analysis::Type::kBool:
                return ty_.bool_();
            default:
                TINT_UNIMPLEMENTED() << "unhandled SPIR-V type: " << type->str();
                return ty_.void_();
        }
    }

    /// @param id a SPIR-V result ID for a type declaration instruction
    /// @returns a Tint type object
    const core::type::Type* Type(uint32_t id) {
        return Type(spirv_context_->get_type_mgr()->GetType(id));
    }

    /// @param id a SPIR-V result ID for a function declaration instruction
    /// @returns a Tint function object
    core::ir::Function* Function(uint32_t id) {
        return functions_.GetOrCreate(id, [&] {
            return b_.Function(ty_.void_(), core::ir::Function::PipelineStage::kUndefined,
                               std::nullopt);
        });
    }

    /// @param id a SPIR-V result ID
    /// @returns a Tint value object
    core::ir::Value* Value(uint32_t id) {
        if (auto* c = spirv_context_->get_constant_mgr()->FindDeclaredConstant(id)) {
            return Constant(c);
        }

        TINT_ASSERT_OR_RETURN_VALUE(values_.Contains(id), nullptr);
        return values_.Get(id).value();
    }

    /// @param constant a SPIR-V constant object
    /// @returns a Tint constant object
    core::ir::Constant* Constant(const spvtools::opt::analysis::Constant* constant) {
        if (auto* bool_ = constant->AsBoolConstant()) {
            return b_.Constant(bool_->value());
        }
        TINT_UNIMPLEMENTED() << "unhandled constant type";
        return nullptr;
    }

    /// Emit the functions.
    void EmitFunctions() {
        for (auto& func : *spirv_context_->module()) {
            Vector<core::ir::FunctionParam*, 4> params;
            func.ForEachParam([&](spvtools::opt::Instruction* spirv_param) {
                auto* param = b_.FunctionParam(Type(spirv_param->type_id()));
                values_.Add(spirv_param->result_id(), param);
                params.Push(param);
            });

            current_function_ = Function(func.result_id());
            current_function_->SetParams(std::move(params));
            current_function_->SetReturnType(Type(func.type_id()));

            functions_.Add(func.result_id(), current_function_);
            EmitBlock(current_function_->Block(), *func.entry());
        }
    }

    /// Emit entry point attributes.
    void EmitEntryPoints() {
        // Handle OpEntryPoint declarations.
        for (auto& entry_point : spirv_context_->module()->entry_points()) {
            auto model = entry_point.GetSingleWordInOperand(0);
            auto* func = Function(entry_point.GetSingleWordInOperand(1));

            // Set the pipeline stage.
            switch (spv::ExecutionModel(model)) {
                case spv::ExecutionModel::GLCompute:
                    func->SetStage(core::ir::Function::PipelineStage::kCompute);
                    break;
                default:
                    TINT_UNIMPLEMENTED() << "unhandled execution model: " << model;
            }

            // Set the entry point name.
            ir_.SetName(func, entry_point.GetOperand(2).AsString());
        }

        // Handle OpExecutionMode declarations.
        for (auto& execution_mode : spirv_context_->module()->execution_modes()) {
            auto* func = functions_.Get(execution_mode.GetSingleWordInOperand(0)).value_or(nullptr);
            auto mode = execution_mode.GetSingleWordInOperand(1);
            TINT_ASSERT_OR_RETURN(func);

            switch (spv::ExecutionMode(mode)) {
                case spv::ExecutionMode::LocalSize:
                    func->SetWorkgroupSize(execution_mode.GetSingleWordInOperand(2),
                                           execution_mode.GetSingleWordInOperand(3),
                                           execution_mode.GetSingleWordInOperand(4));
                    break;
                default:
                    TINT_UNIMPLEMENTED() << "unhandled execution mode: " << mode;
            }
        }
    }

    /// Emit the contents of SPIR-V block @p src into Tint IR block @p dst.
    /// @param dst the Tint IR block to append to
    /// @param src the SPIR-V block to emit
    void EmitBlock(core::ir::Block* dst, const spvtools::opt::BasicBlock& src) {
        for (auto& inst : src) {
            switch (inst.opcode()) {
                case spv::Op::OpFunctionCall:
                    dst->Append(EmitFunctionCall(inst));
                    break;
                case spv::Op::OpReturn:
                    dst->Append(b_.Return(current_function_));
                    break;
                case spv::Op::OpReturnValue:
                    dst->Append(b_.Return(current_function_, Value(inst.GetSingleWordOperand(0))));
                    break;
                default:
                    TINT_UNIMPLEMENTED()
                        << "unhandled SPIR-V instruction: " << static_cast<uint32_t>(inst.opcode());
            }
        }
    }

    /// @param inst the SPIR-V instruction for OpFunctionCall
    /// @returns the Tint IR instruction
    core::ir::UserCall* EmitFunctionCall(const spvtools::opt::Instruction& inst) {
        // TODO(crbug.com/tint/1907): Capture result.
        Vector<core::ir::Value*, 4> args;
        for (uint32_t i = 3; i < inst.NumOperandWords(); i++) {
            args.Push(Value(inst.GetSingleWordOperand(i)));
        }
        return b_.Call(Function(inst.GetSingleWordInOperand(0)), std::move(args));
    }

  private:
    core::ir::Module ir_;
    core::ir::Builder b_{ir_};
    core::type::Manager& ty_{ir_.Types()};

    core::ir::Function* current_function_ = nullptr;
    Hashmap<uint32_t, core::ir::Function*, 8> functions_;
    Hashmap<uint32_t, core::ir::Value*, 8> values_;

    std::unique_ptr<spvtools::opt::IRContext> spirv_context_;
};

}  // namespace

Result<core::ir::Module> Parse(Slice<const uint32_t> spirv) {
    return Parser{}.Run(spirv);
}

}  // namespace tint::spirv::reader
