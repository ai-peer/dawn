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

#include "src/tint/lang/glsl/writer/printer/printer.h"

#include <utility>

#include "src/tint/lang/core/ir/return.h"
#include "src/tint/lang/core/ir/unreachable.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/utils/macros/scoped_assignment.h"
#include "src/tint/utils/rtti/switch.h"

using namespace tint::core::fluent_types;  // NOLINT

namespace tint::glsl::writer {

// Helper for calling TINT_UNIMPLEMENTED() from a Switch(object_ptr) default case.
#define UNHANDLED_CASE(object_ptr)                         \
    TINT_UNIMPLEMENTED() << "unhandled case in Switch(): " \
                         << (object_ptr ? object_ptr->TypeInfo().name : "<null>")

Printer::Printer(core::ir::Module& module) : ir_(module) {}

Printer::~Printer() = default;

tint::Result<SuccessType> Printer::Generate(Version version) {
    auto valid = core::ir::ValidateAndDumpIfNeeded(ir_, "GLSL writer");
    if (!valid) {
        return std::move(valid.Failure());
    }

    {
        TINT_SCOPED_ASSIGNMENT(current_buffer_, &preamble_buffer_);

        auto out = Line();
        out << "#version " << version.major_version << version.minor_version << "0";
        if (version.IsES()) {
            out << " es";
        }
    }

    // Emit module-scope declarations.
    EmitBlockInstructions(ir_.root_block);

    // Emit functions.
    for (auto* func : ir_.functions) {
        EmitFunction(func);
    }

    return Success;
}

std::string Printer::Result() const {
    StringStream ss;
    ss << preamble_buffer_.String() << '\n' << main_buffer_.String();
    return ss.str();
}

void Printer::EmitFunction(core::ir::Function* func) {
    TINT_SCOPED_ASSIGNMENT(current_function_, func);

    {
        auto out = Line();

        // TODO(dsinclair): Emit function stage if any
        // TODO(dsinclair): Handle return type attributes

        EmitType(out, func->ReturnType());
        out << " " << ir_.NameOf(func).Name() << "() {";

        // TODO(dsinclair): Emit Function parameters
    }
    {
        ScopedIndent si(current_buffer_);
        EmitBlock(func->Block());
    }

    Line() << "}";
}

void Printer::EmitBlock(core::ir::Block* block) {
    // TODO(dsinclair): Handle marking inline
    // MarkInlinable(block);

    EmitBlockInstructions(block);
}

void Printer::EmitBlockInstructions(core::ir::Block* block) {
    TINT_SCOPED_ASSIGNMENT(current_block_, block);

    for (auto* inst : *block) {
        Switch(
            inst,                                                //
            [&](core::ir::Return* r) { EmitReturn(r); },         //
            [&](core::ir::Unreachable*) { EmitUnreachable(); },  //
            [&](Default) { UNHANDLED_CASE(inst); });
    }
}

void Printer::EmitType(StringStream& out, [[maybe_unused]] const core::type::Type* ty) {
    out << "void";
}

void Printer::EmitReturn(core::ir::Return* r) {
    // If this return has no arguments and the current block is for the function which is
    // being returned, skip the return.
    if (current_block_ == current_function_->Block() && r->Args().IsEmpty()) {
        return;
    }

    auto out = Line();
    out << "return";
    // TODO(dsinclair): Handle return args
    // if (!r->Args().IsEmpty()) {
    //     out << " " << Expr(r->Args().Front());
    // }
    out << ";";
}

void Printer::EmitUnreachable() {
    Line() << "/* unreachable */";
}

}  // namespace tint::glsl::writer
