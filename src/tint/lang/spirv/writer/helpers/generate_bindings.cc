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

#include "src/tint/lang/spirv/writer/helpers/generate_bindings.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/tint/api/common/binding_point.h"
#include "src/tint/lang/core/type/external_texture.h"
#include "src/tint/lang/core/type/storage_texture.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/program/program.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/utils/containers/hashmap.h"
#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/rtti/switch.h"

namespace tint::spirv::writer {

Bindings GenerateBindings(const Program& program) {
    // TODO(tint:1491): Use Inspector once we can get binding info for all
    // variables, not just those referenced by entry points.

    Bindings bindings{};

    std::unordered_set<tint::BindingPoint> seen_binding_points;

    // This is a bit of a hack. The binding points must be unique over all the `binding`
    // entries. But, this is looking at _all_ entry points where bindings can overlap.
    // In the case where both entry points used the same type (uniform, sampler, etc)
    // then it would be fine as it just overwrites with itself. But, if the two entries
    // have different types, this can lead to missing bindings.
    //
    // To work around this, all duplicate bindings are remapped. This has the downside
    // that multiple entry points in a single file, the later entry points will have
    // all their bindings changed. Using the SingleEntryPoint transform would be
    // the way around this, there is only a single entry point so it will keep its
    // bindings intact.
    auto find_bind_point = [&](uint32_t group, uint32_t binding) -> binding::BindingInfo {
        tint::BindingPoint bp{group, binding};
        while (seen_binding_points.find(bp) != seen_binding_points.end()) {
            bp.binding += 1;
        }
        seen_binding_points.emplace(bp);
        return binding::BindingInfo{bp.group, bp.binding};
    };

    Vector<tint::BindingPoint, 4> ext_tex_bps;
    for (auto* var : program.AST().GlobalVariables()) {
        if (auto* sem_var = program.Sem().Get(var)->As<sem::GlobalVariable>()) {
            if (auto bp = sem_var->BindingPoint()) {
                // Store up the external textures, we'll add them in the next step
                if (sem_var->Type()->UnwrapRef()->Is<core::type::ExternalTexture>()) {
                    ext_tex_bps.Push(*bp);
                    continue;
                }

                binding::BindingInfo info = find_bind_point(bp->group, bp->binding);
                switch (sem_var->AddressSpace()) {
                    case core::AddressSpace::kHandle:
                        Switch(
                            sem_var->Type()->UnwrapRef(),  //
                            [&](const core::type::Sampler*) {
                                bindings.sampler.emplace(*bp, info);
                            },
                            [&](const core::type::StorageTexture*) {
                                bindings.storage_texture.emplace(*bp, info);
                            },
                            [&](const core::type::Texture*) {
                                bindings.texture.emplace(*bp, info);
                            });
                        break;
                    case core::AddressSpace::kStorage:
                        bindings.storage.emplace(*bp, info);
                        break;
                    case core::AddressSpace::kUniform:
                        bindings.uniform.emplace(*bp, info);
                        break;

                    case core::AddressSpace::kUndefined:
                    case core::AddressSpace::kPixelLocal:
                    case core::AddressSpace::kPrivate:
                    case core::AddressSpace::kPushConstant:
                    case core::AddressSpace::kIn:
                    case core::AddressSpace::kOut:
                    case core::AddressSpace::kFunction:
                    case core::AddressSpace::kWorkgroup:
                        break;
                }
            }
        }
    }

    for (auto bp : ext_tex_bps) {
        binding::BindingInfo plane0 = find_bind_point(bp.group, bp.binding);
        binding::BindingInfo plane1 = find_bind_point(bp.group, bp.binding);
        binding::BindingInfo metadata = find_bind_point(bp.group, bp.binding);

        bindings.external_texture.emplace(bp, binding::ExternalTexture{metadata, plane0, plane1});
    }

    return bindings;
}

}  // namespace tint::spirv::writer
