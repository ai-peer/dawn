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

#include "src/tint/lang/core/constant/splat.h"
#include "src/tint/lang/core/ir/binary.h"
#include "src/tint/lang/core/ir/constant.h"
#include "src/tint/lang/core/ir/exit_if.h"
#include "src/tint/lang/core/ir/if.h"
#include "src/tint/lang/core/ir/let.h"
#include "src/tint/lang/core/ir/load.h"
#include "src/tint/lang/core/ir/multi_in_block.h"
#include "src/tint/lang/core/ir/return.h"
#include "src/tint/lang/core/ir/unreachable.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/ir/var.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/atomic.h"
#include "src/tint/lang/core/type/bool.h"
#include "src/tint/lang/core/type/depth_multisampled_texture.h"
#include "src/tint/lang/core/type/depth_texture.h"
#include "src/tint/lang/core/type/external_texture.h"
#include "src/tint/lang/core/type/f16.h"
#include "src/tint/lang/core/type/f32.h"
#include "src/tint/lang/core/type/i32.h"
#include "src/tint/lang/core/type/matrix.h"
#include "src/tint/lang/core/type/multisampled_texture.h"
#include "src/tint/lang/core/type/pointer.h"
#include "src/tint/lang/core/type/sampled_texture.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/core/type/texture.h"
#include "src/tint/lang/core/type/u32.h"
#include "src/tint/lang/core/type/vector.h"
#include "src/tint/lang/core/type/void.h"
#include "src/tint/utils/containers/map.h"
#include "src/tint/utils/macros/scoped_assignment.h"
#include "src/tint/utils/rtti/switch.h"
#include "src/tint/utils/strconv/float_to_string.h"
#include "src/tint/utils/text/string.h"

using namespace tint::core::fluent_types;  // NOLINT

namespace tint::glsl::writer {
namespace {

void PrintI32(StringStream& out, int32_t value) {
    // GLSL parses `-2147483648` as a unary minus and `2147483648` as separate tokens, and the
    // latter doesn't fit into an (32-bit) `int`. Emit `(-2147483647 - 1)` instead, which ensures
    // the expression type is `int`.
    if (auto int_min = std::numeric_limits<int32_t>::min(); value == int_min) {
        out << "(" << int_min + 1 << " - 1)";
    } else {
        out << value;
    }
}

void PrintF32(StringStream& out, float value) {
    if (std::isinf(value)) {
        out << "0.0f " << (value >= 0 ? "/* inf */" : "/* -inf */");
    } else if (std::isnan(value)) {
        out << "0.0f /* nan */";
    } else {
        out << tint::writer::FloatToString(value) << "f";
    }
}

void PrintF16(StringStream& out, float value) {
    if (std::isinf(value)) {
        out << "0.0hf " << (value >= 0 ? "/* inf */" : "/* -inf */");
    } else if (std::isnan(value)) {
        out << "0.0hf /* nan */";
    } else {
        out << tint::writer::FloatToString(value) << "hf";
    }
}

// Helper for calling TINT_UNIMPLEMENTED() from a Switch(object_ptr) default case.
#define UNHANDLED_CASE(object_ptr)                         \
    TINT_UNIMPLEMENTED() << "unhandled case in Switch(): " \
                         << (object_ptr ? object_ptr->TypeInfo().name : "<null>")

}  // namespace

Printer::Printer(core::ir::Module& module, const Version& version)
    : ir_(module), version_(version) {}

Printer::~Printer() = default;

tint::Result<SuccessType> Printer::Generate() {
    auto valid = core::ir::ValidateAndDumpIfNeeded(ir_, "GLSL writer");
    if (!valid) {
        return std::move(valid.Failure());
    }

    {
        TINT_SCOPED_ASSIGNMENT(current_buffer_, &preamble_buffer_);
        auto out = Line();
        out << "#version " << version_.major_version << version_.minor_version << "0";
        if (version_.IsES()) {
            out << " es";
        }
    }

    // TODO(dsinclair): Record Enables ....

    // Emit module-scope declarations.
    EmitBlockInstructions(ir_.root_block);

    // Emit functions.
    for (auto* func : ir_.functions) {
        EmitFunction(func);
    }

    {
        TINT_SCOPED_ASSIGNMENT(current_buffer_, &preamble_buffer_);

        if (version_.IsES() && requires_oes_sample_variables_) {
            Line() << "#extension GL_OES_sample_variables : require";
        }

        if (requires_f16_extension_) {
            Line() << "#extension GL_AMD_gpu_shader_half_float : require";
        }

        if (requires_dual_source_blending_extension_) {
            Line() << "#extension GL_EXT_blend_func_extended : require";
        }

        if (version_.IsES() && requires_default_precision_qualifier_) {
            Line() << "precision highp float;";
        }
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
    MarkInlinable(block);

    EmitBlockInstructions(block);
}

void Printer::EmitBlockInstructions(core::ir::Block* block) {
    TINT_SCOPED_ASSIGNMENT(current_block_, block);

    for (auto* inst : *block) {
        Switch(
            inst,                                         //
            [&](core::ir::Let* l) { EmitLet(l); },        //
            [&](core::ir::Return* r) { EmitReturn(r); },  //
            [&](core::ir::Var* v) { EmitVar(v); },        //
            [&](Default) { TINT_ICE() << "unimplemented instruction: " << inst->TypeInfo().name; });
    }
}

void Printer::EmitVar(core::ir::Var* v) {
    auto out = Line();

    auto* ptr = v->Result()->Type()->As<core::type::Pointer>();
    TINT_ASSERT_OR_RETURN(ptr);

    auto name = ir_.NameOf(v);

    EmitType(out, ptr->UnwrapPtr());
    out << " " << name.Name();

    auto space = ptr->AddressSpace();
    if (v->Initializer()) {
        out << " = " << Expr(v->Initializer());
    } else if (space == core::AddressSpace::kPrivate || space == core::AddressSpace::kFunction ||
               space == core::AddressSpace::kUndefined) {
        out << " = ";
        EmitZeroValue(out, ptr->UnwrapPtr());
    }
    out << ";";

    Bind(v->Result(), name, PtrKind::kRef);
}

void Printer::EmitLet(core::ir::Let* l) {
    Bind(l->Result(), Expr(l->Value(), PtrKind::kPtr), PtrKind::kPtr);
}

void Printer::EmitZeroValue(StringStream& out, const core::type::Type* ty) {
    Switch(
        ty, [&](const core::type::Bool*) { out << "false"; },  //
        [&](const core::type::F16*) { out << "0.0hf"; },       //
        [&](const core::type::F32*) { out << "0.0f"; },        //
        [&](const core::type::I32*) { out << "0"; },           //
        [&](const core::type::U32*) { out << "0u"; },          //
        [&](const core::type::Vector* vec) {
            EmitType(out, vec);
            ScopedParen sp(out);
            for (uint32_t i = 0; i < vec->Width(); i++) {
                if (i != 0) {
                    out << ", ";
                }
                EmitZeroValue(out, vec->type());
            }
        },  //
        [&](const core::type::Matrix* mat) {
            EmitType(out, mat);
            ScopedParen sp(out);
            for (uint32_t i = 0; i < (mat->rows() * mat->columns()); i++) {
                if (i != 0) {
                    out << ", ";
                }
                EmitZeroValue(out, mat->type());
            }
        },
        [&](const core::type::Array* arr) {
            EmitType(out, arr);
            ScopedParen sp(out);

            auto count = arr->ConstantCount();
            if (!count) {
                diagnostics_.add_error(diag::System::Writer,
                                       core::type::Array::kErrExpectedConstantCount);
                return;
            }

            for (uint32_t i = 0; i < count; i++) {
                if (i != 0) {
                    out << ", ";
                }
                EmitZeroValue(out, arr->ElemType());
            }
        },  //
        [&](const core::type::Struct* s) {
            EmitType(out, s);
            bool first = true;
            ScopedParen sp(out);
            for (auto* member : s->Members()) {
                if (!first) {
                    out << ", ";
                } else {
                    first = false;
                }
                EmitZeroValue(out, member->Type());
            }
        },  //
        [&](Default) { TINT_ICE() << "Invalid type for zero emission: " << ty->FriendlyName(); });
}

void Printer::EmitType(StringStream& out, const core::type::Type* ty) {
    tint::Switch(
        ty,                                                   //
        [&](const core::type::Bool*) { out << "bool"; },      //
        [&](const core::type::Void*) { out << "void"; },      //
        [&](const core::type::F32*) { out << "float"; },      //
        [&](const core::type::F16*) { out << "float16_t"; },  //
        [&](const core::type::I32*) { out << "int"; },        //
        [&](const core::type::U32*) { out << "uint"; },       //
        [&](const core::type::Array* arr) { EmitArrayType(out, arr); },
        [&](const core::type::Vector* vec) { EmitVectorType(out, vec); },
        [&](const core::type::Matrix* mat) { EmitMatrixType(out, mat); },
        [&](const core::type::Atomic* atomic) { EmitAtomicType(out, atomic); },
        [&](const core::type::Pointer* ptr) { EmitPointerType(out, ptr); },
        [&](const core::type::Sampler*) { out << "sampler"; },  //
        [&](const core::type::Texture* tex) { EmitTextureType(out, tex); },
        [&](const core::type::Struct* str) {
            out << StructName(str);

            TINT_SCOPED_ASSIGNMENT(current_buffer_, &preamble_buffer_);
            EmitStructType(str);
        },
        [&](Default) { UNHANDLED_CASE(ty); });
}

void Printer::EmitAddressSpace([[maybe_unused]] StringStream& out,
                               [[maybe_unused]] core::AddressSpace sc) {}

void Printer::EmitPointerType([[maybe_unused]] StringStream& out,
                              [[maybe_unused]] const core::type::Pointer* ptr) {}

void Printer::EmitAtomicType([[maybe_unused]] StringStream& out,
                             [[maybe_unused]] const core::type::Atomic* atomic) {}

void Printer::EmitArrayType([[maybe_unused]] StringStream& out,
                            [[maybe_unused]] const core::type::Array* ary) {}

void Printer::EmitVectorType([[maybe_unused]] StringStream& out,
                             [[maybe_unused]] const core::type::Vector* vec) {}

void Printer::EmitMatrixType([[maybe_unused]] StringStream& out,
                             [[maybe_unused]] const core::type::Matrix* mat) {}

void Printer::EmitTextureType([[maybe_unused]] StringStream& out,
                              [[maybe_unused]] const core::type::Texture* tex) {}

std::string Printer::StructName(const core::type::Struct* s) {
    auto name = s->Name().Name();
    if (HasPrefix(name, "__")) {
        name = tint::GetOrCreate(builtin_struct_names_, s,
                                 [&] { return UniqueIdentifier(name.substr(2)); });
    }
    return name;
}

std::string Printer::UniqueIdentifier(const std::string& prefix /* = "" */) {
    return ir_.symbols.New(prefix).Name();
}

void Printer::EmitStructType(const core::type::Struct* str) {
    auto it = emitted_structs_.emplace(str);
    if (!it.second) {
        return;
    }

    // This does not append directly to the preamble because a struct may require other
    // structs, or the array template, to get emitted before it. So, the struct emits into a
    // temporary text buffer, then anything it depends on will emit to the preamble first,
    // and then it copies the text buffer into the preamble.
    TextBuffer str_buf;
    Line(&str_buf) << "struct " << StructName(str) << " {";

    str_buf.IncrementIndent();

    for (auto* mem : str->Members()) {
        auto mem_name = mem->Name().Name();
        auto* ty = mem->Type();

        auto out = Line(&str_buf);
        EmitType(out, ty);
        out << " " << mem_name << ";";
    }

    str_buf.DecrementIndent();
    Line(&str_buf) << "};";

    preamble_buffer_.Append(str_buf);
}

void Printer::EmitConstant(StringStream& out, core::ir::Constant* c) {
    EmitConstant(out, c->Value());
}

void Printer::EmitConstant(StringStream& out, const core::constant::Value* c) {
    auto emit_values = [&](uint32_t count) {
        for (size_t i = 0; i < count; i++) {
            if (i > 0) {
                out << ", ";
            }
            EmitConstant(out, c->Index(i));
        }
    };

    tint::Switch(
        c->Type(),  //
        [&](const core::type::Bool*) { out << (c->ValueAs<bool>() ? "true" : "false"); },
        [&](const core::type::I32*) { PrintI32(out, c->ValueAs<i32>()); },
        [&](const core::type::U32*) { out << c->ValueAs<u32>() << "u"; },
        [&](const core::type::F32*) { PrintF32(out, c->ValueAs<f32>()); },
        [&](const core::type::F16*) { PrintF16(out, c->ValueAs<f16>()); },
        [&](const core::type::Vector* v) {
            ScopedParen sp(out);

            if (auto* splat = c->As<core::constant::Splat>()) {
                EmitConstant(out, splat->el);
                return;
            }
            emit_values(v->Width());
        },
        [&](const core::type::Matrix* m) {
            EmitType(out, m);
            ScopedParen sp(out);
            emit_values(m->columns());
        },
        [&](const core::type::Array* a) {
            EmitType(out, a);
            ScopedParen sp(out);

            auto count = a->ConstantCount();
            if (!count) {
                diagnostics_.add_error(diag::System::Writer,
                                       core::type::Array::kErrExpectedConstantCount);
                return;
            }
            emit_values(*count);
        },
        [&](const core::type::Struct* s) {
            EmitStructType(s);
            out << StructName(s);

            ScopedParen sp(out);
            emit_values(static_cast<uint32_t>(s->Members().Length()));
        },
        [&](Default) { UNHANDLED_CASE(c->Type()); });
}

void Printer::EmitReturn(core::ir::Return* r) {
    // If this return has no arguments and the current block is for the function which is
    // being returned, skip the return.
    if (current_block_ == current_function_->Block() && r->Args().IsEmpty()) {
        return;
    }

    auto out = Line();
    out << "return";
    if (!r->Args().IsEmpty()) {
        out << " " << Expr(r->Args().Front());
    }
    out << ";";
}

void Printer::EmitUnreachable() {
    Line() << "/* unreachable */";
}

std::string Printer::Expr(core::ir::Value* value, PtrKind want_ptr_kind) {
    using ExprAndPtrKind = std::pair<std::string, PtrKind>;

    auto [expr, got_ptr_kind] = tint::Switch(
        value,
        [&](core::ir::Constant* c) -> ExprAndPtrKind {
            StringStream str;
            EmitConstant(str, c);
            return {str.str(), PtrKind::kRef};
        },
        [&](Default) -> ExprAndPtrKind {
            auto lookup = bindings_.Find(value);
            if (TINT_UNLIKELY(!lookup)) {
                TINT_ICE() << "Expr(" << (value ? value->TypeInfo().name : "null")
                           << ") value has no expression";
                return {};
            }

            return std::visit(
                [&](auto&& got) -> ExprAndPtrKind {
                    using T = std::decay_t<decltype(got)>;

                    if constexpr (std::is_same_v<T, VariableValue>) {
                        return {got.name.Name(), got.ptr_kind};
                    }

                    if constexpr (std::is_same_v<T, InlinedValue>) {
                        auto result = ExprAndPtrKind{got.expr, got.ptr_kind};

                        // Single use (inlined) expression.
                        // Mark the bindings_ map entry as consumed.
                        *lookup = ConsumedValue{};
                        return result;
                    }

                    if constexpr (std::is_same_v<T, ConsumedValue>) {
                        TINT_ICE() << "Expr(" << value->TypeInfo().name
                                   << ") called twice on the same value";
                    } else {
                        TINT_ICE() << "Expr(" << value->TypeInfo().name << ") has unhandled value";
                    }
                    return {};
                },
                *lookup);
        });
    if (expr.empty()) {
        return "<error>";
    }

    if (value->Type()->Is<core::type::Pointer>()) {
        return ToPtrKind(expr, got_ptr_kind, want_ptr_kind);
    }

    return expr;
}

std::string Printer::ToPtrKind(const std::string& in, PtrKind got, PtrKind want) {
    if (want == PtrKind::kRef && got == PtrKind::kPtr) {
        return "*(" + in + ")";
    }
    if (want == PtrKind::kPtr && got == PtrKind::kRef) {
        return "&(" + in + ")";
    }
    return in;
}

void Printer::Bind(core::ir::Value* value, const std::string& expr, PtrKind ptr_kind) {
    TINT_ASSERT(value);

    if (can_inline_.Remove(value)) {
        // Value will be inlined at its place of usage.
        if (TINT_LIKELY(bindings_.Add(value, InlinedValue{expr, ptr_kind}))) {
            return;
        }
    } else {
        auto mod_name = ir_.NameOf(value);
        if (value->Usages().IsEmpty() && !mod_name.IsValid()) {
            // Drop phonies.
        } else {
            if (mod_name.Name().empty()) {
                mod_name = ir_.symbols.New("v");
            }

            auto out = Line();
            EmitType(out, value->Type());
            out << " const " << mod_name.Name() << " = ";
            if (value->Type()->Is<core::type::Pointer>()) {
                out << ToPtrKind(expr, ptr_kind, PtrKind::kPtr);
            } else {
                out << expr;
            }
            out << ";";

            Bind(value, mod_name, PtrKind::kPtr);
        }
        return;
    }

    TINT_ICE() << "Bind(" << value->TypeInfo().name << ") called twice for same value";
}

void Printer::Bind(core::ir::Value* value, Symbol name, PtrKind ptr_kind) {
    TINT_ASSERT(value);

    bool added = bindings_.Add(value, VariableValue{name, ptr_kind});
    if (TINT_UNLIKELY(!added)) {
        TINT_ICE() << "Bind(" << value->TypeInfo().name << ") called twice for same value";
    }
}

void Printer::MarkInlinable(core::ir::Block* block) {
    // An ordered list of possibly-inlinable values returned by sequenced instructions that have
    // not yet been marked-for or ruled-out-for inlining.
    UniqueVector<core::ir::Value*, 32> pending_resolution;

    // Walk the instructions of the block starting with the first.
    for (auto* inst : *block) {
        // Is the instruction sequenced?
        bool sequenced = inst->Sequenced();

        if (inst->Results().Length() != 1) {
            continue;
        }

        // Instruction has a single result value.
        // Check to see if the result of this instruction is a candidate for inlining.
        auto* result = inst->Result();
        // Only values with a single usage can be inlined.
        // Named values are not inlined, as we want to emit the name for a let.
        if (result->Usages().Count() == 1 && !ir_.NameOf(result).IsValid()) {
            if (sequenced) {
                // The value comes from a sequenced instruction.  Don't inline.
            } else {
                // The value comes from an unsequenced instruction. Just inline.
                can_inline_.Add(result);
            }
            continue;
        }
    }
}

}  // namespace tint::glsl::writer
