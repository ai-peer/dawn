// Copyright 2022 The Tint Authors.
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

#include "src/tint/transform/pad_structs.h"

#include "src/tint/program_builder.h"
#include "src/tint/sem/module.h"

TINT_INSTANTIATE_TYPEINFO(tint::transform::PadStructs);

namespace tint::transform {

namespace {
const ast::StructMember* CreatePadding(ProgramBuilder* b, uint32_t bytes) {
    auto name = b->Symbols().New("padding");
    const ast::Type* type = b->ty.f32();
    return b->Member(name, type);
}

}

PadStructs::PadStructs() = default;

PadStructs::~PadStructs() = default;

void PadStructs::Run(CloneContext& ctx, const DataMap& inputs, DataMap&) const {
    auto& sem = ctx.src->Sem();

    ctx.ReplaceAll([&](const ast::Struct* ast_str) -> const ast::Struct* {
        auto* str = sem.Get<sem::Struct>(ast_str);
        if (!str) {
            return nullptr;
        }
        uint32_t offset = 0;
        bool host_shareable = str->IsHostShareable();
        bool has_runtime_sized_array = false;
        utils::Vector<const ast::StructMember*, 8> new_members;
        for (auto* mem : str->Members()) {
            auto name = ctx.src->Symbols().NameFor(mem->Name());

            if (host_shareable && offset < mem->Offset()) {
                new_members.Push(CreatePadding(ctx.dst, mem->Offset() - offset));
                offset = mem->Offset();
            }

            auto* ty = mem->Type();
            const ast::Type* type = CreateASTTypeFor(ctx, ty);
            new_members.Push(ctx.dst->Member(name, type));

            if (host_shareable) {
                uint32_t size = ty->Size();
                if (ty->Is<sem::Struct>()) {
                    // GLSL structs are already padded out to a multiple of 16.
                    size = utils::RoundUp(16u, size);
                } else if (auto* array_ty = ty->As<sem::Array>()) {
                    if (array_ty->Count() == 0) {
                        has_runtime_sized_array = true;
                    }
                }
                offset += size;
            }
        }
        if (host_shareable && offset < str->Size() && !has_runtime_sized_array) {
            new_members.Push(CreatePadding(ctx.dst, str->Size() - offset));
        }
        return ctx.dst->Structure(ctx.Clone(str->Name()), std::move(new_members));
    });
}

}  // namespace tint::transform
