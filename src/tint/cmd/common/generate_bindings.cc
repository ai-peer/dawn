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

#include "src/tint/cmd/common/generate_bindings.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "src/tint/api/common/binding_point.h"
#include "src/tint/lang/core/type/external_texture.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/wgsl/program/program.h"
#include "src/tint/lang/wgsl/sem/variable.h"

namespace tint::cmd {

#if TINT_BUILD_SPV_WRITER
tint::spirv::writer::Bindings GenerateSpirvBindings(const Program& program) {
    // TODO(tint:1491): Use Inspector once we can get binding info for all
    // variables, not just those referenced by entry points.

    tint::spirv::writer::Bindings bindings{};

    using BindingInfo = tint::spirv::binding::BindingInfo;

    // Collect next valid binding number per group
    std::unordered_map<uint32_t, uint32_t> group_to_next_binding_number;
    std::vector<tint::BindingPoint> ext_tex_bps;
    for (auto* var : program.AST().GlobalVariables()) {
        if (auto* sem_var = program.Sem().Get(var)->As<sem::GlobalVariable>()) {
            if (auto bp = sem_var->BindingPoint()) {
                auto& n = group_to_next_binding_number[bp->group];
                n = std::max(n, bp->binding + 1);

                // Store up the external textures, we'll add them in the next step
                if (sem_var->Type()->UnwrapRef()->Is<core::type::ExternalTexture>()) {
                    ext_tex_bps.emplace_back(*bp);
                    continue;
                }

                BindingInfo info{bp->group, bp->binding};
                switch (sem_var->AddressSpace()) {
                    case core::AddressSpace::kHandle:
                        Switch(
                            sem_var->Type()->UnwrapRef(),  //
                            [&](core::type::Sampler* sampler) {
                                bindings.sampler.emplace(*bp, info);
                            },
                            [&](core::type::Texture* texture) {
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

    using ExternalTexture = tint::spirv::binding::ExternalTexture;
    for (auto bp : ext_tex_bps) {
        uint32_t g = bp.group;
        uint32_t& next_num = group_to_next_binding_number[g];

        BindingInfo plane0{bp.group, bp.binding};
        BindingInfo plane1{g, next_num++};
        BindingInfo metadata{g, next_num++};

        bindings.external_texture.emplace(bp, ExternalTexture{metadata, plane0, plane1});
    }

    return bindings;
}
#endif  // TINT_BUILD_SPV_WRITER

}  // namespace tint::cmd
