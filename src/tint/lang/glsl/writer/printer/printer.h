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

#ifndef SRC_TINT_LANG_GLSL_WRITER_PRINTER_PRINTER_H_
#define SRC_TINT_LANG_GLSL_WRITER_PRINTER_PRINTER_H_

#include <string>

#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/glsl/writer/common/version.h"
#include "src/tint/utils/generator/text_generator.h"

// Forward declarations
namespace tint::core::ir {
class Binary;
class ExitIf;
class If;
class Let;
class Load;
class Return;
class Unreachable;
class Var;
}  // namespace tint::core::ir

namespace tint::glsl::writer {

/// Implementation class for the MSL generator
class Printer : public tint::TextGenerator {
  public:
    /// Constructor
    /// @param module the Tint IR module to generate
    explicit Printer(core::ir::Module& module);
    ~Printer() override;

    /// @param version the GLSL version information
    /// @returns success or failure
    tint::Result<SuccessType> Generate(Version version);

    /// @copydoc tint::TextGenerator::Result
    std::string Result() const override;

  private:
    /// Emit the function
    /// @param func the function to emit
    void EmitFunction(core::ir::Function* func);

    /// Emit a block
    /// @param block the block to emit
    void EmitBlock(core::ir::Block* block);
    /// Emit the instructions in a block
    /// @param block the block with the instructions to emit
    void EmitBlockInstructions(core::ir::Block* block);

    /// Emit a return instruction
    /// @param r the return instruction
    void EmitReturn(core::ir::Return* r);
    /// Emit an unreachable instruction
    void EmitUnreachable();

    /// Emit a type
    /// @param out the stream to emit too
    /// @param ty the type to emit
    void EmitType(StringStream& out, const core::type::Type* ty);

    core::ir::Module& ir_;

    /// The buffer holding preamble text
    TextBuffer preamble_buffer_;

    /// The current function being emitted
    core::ir::Function* current_function_ = nullptr;
    /// The current block being emitted
    core::ir::Block* current_block_ = nullptr;
};

}  // namespace tint::glsl::writer

#endif  // SRC_TINT_LANG_GLSL_WRITER_PRINTER_PRINTER_H_
