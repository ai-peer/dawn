// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/hlsl/writer/raise/raise.h"

#include <unordered_set>

#include "src/tint/lang/core/ir/transform/add_empty_entry_point.h"
#include "src/tint/lang/core/ir/transform/array_length_from_uniform.h"
#include "src/tint/lang/core/ir/transform/binary_polyfill.h"
#include "src/tint/lang/core/ir/transform/binding_remapper.h"
#include "src/tint/lang/core/ir/transform/builtin_polyfill.h"
#include "src/tint/lang/core/ir/transform/conversion_polyfill.h"
#include "src/tint/lang/core/ir/transform/demote_to_helper.h"
#include "src/tint/lang/core/ir/transform/multiplanar_external_texture.h"
#include "src/tint/lang/core/ir/transform/remove_terminator_args.h"
#include "src/tint/lang/core/ir/transform/rename_conflicts.h"
#include "src/tint/lang/core/ir/transform/robustness.h"
#include "src/tint/lang/core/ir/transform/value_to_let.h"
#include "src/tint/lang/core/ir/transform/vectorize_scalar_matrix_constructors.h"
#include "src/tint/lang/core/ir/transform/zero_init_workgroup_memory.h"
#include "src/tint/lang/hlsl/writer/common/option_helpers.h"
#include "src/tint/lang/hlsl/writer/common/options.h"
#include "src/tint/lang/hlsl/writer/raise/builtin_polyfill.h"
#include "src/tint/lang/hlsl/writer/raise/decompose_memory_access.h"
#include "src/tint/lang/hlsl/writer/raise/fxc_polyfill.h"
#include "src/tint/lang/hlsl/writer/raise/promote_initializers.h"
#include "src/tint/lang/hlsl/writer/raise/shader_io.h"
#include "src/tint/utils/result/result.h"

namespace tint::hlsl::writer {

Result<SuccessType> Raise(core::ir::Module& module, const Options& options) {
#define RUN_TRANSFORM(name, ...)                   \
    do {                                           \
        auto result = name(module, ##__VA_ARGS__); \
        if (result != Success) {                   \
            return result;                         \
        }                                          \
    } while (false)

    tint::transform::multiplanar::BindingsMap multiplanar_map{};
    RemapperData remapper_data{};
    ArrayLengthFromUniformOptions array_length_from_uniform_options{};
    PopulateBindingRelatedOptions(options, remapper_data, multiplanar_map,
                                  array_length_from_uniform_options);

    {
        auto result = core::ir::transform::ArrayLengthFromUniform(
            module,
            BindingPoint{array_length_from_uniform_options.ubo_binding.group,
                         array_length_from_uniform_options.ubo_binding.binding},
            array_length_from_uniform_options.bindpoint_to_size_index);
        if (result != Success) {
            return Failure();
        }
    }

    RUN_TRANSFORM(core::ir::transform::BindingRemapper, remapper_data);
    RUN_TRANSFORM(core::ir::transform::MultiplanarExternalTexture, multiplanar_map);

    {
        core::ir::transform::BinaryPolyfillConfig binary_polyfills{};
        binary_polyfills.int_div_mod = !options.disable_polyfill_integer_div_mod;
        binary_polyfills.bitshift_modulo = true;
        RUN_TRANSFORM(core::ir::transform::BinaryPolyfill, binary_polyfills);
    }

    {
        // TODO(dsinclair): Add missing polyfills

        core::ir::transform::BuiltinPolyfillConfig core_polyfills{};
        // core_polyfills.acosh = ast::transform::BuiltinPolyfill::Level::kFull;
        // core_polyfills.asinh = true;
        // core_polyfills.atanh = ast::transform::BuiltinPolyfill::Level::kFull;
        // core_polyfills.bitshift_modulo = true;
        core_polyfills.clamp_int = true;
        core_polyfills.dot_4x8_packed = options.polyfill_dot_4x8_packed;

        // TODO(crbug.com/tint/1449): Some of these can map to HLSL's `firstbitlow`
        // and `firstbithigh`.
        core_polyfills.count_leading_zeros = true;
        core_polyfills.count_trailing_zeros = true;
        core_polyfills.extract_bits = core::ir::transform::BuiltinPolyfillLevel::kFull;
        core_polyfills.first_leading_bit = true;
        core_polyfills.first_trailing_bit = true;
        // core_polyfills.fwidth_fine = true;
        core_polyfills.insert_bits = core::ir::transform::BuiltinPolyfillLevel::kFull;
        // core_polyfills.int_div_mod = !options.disable_polyfill_integer_div_mod;

        // Currently Pack4xU8Clamp() must be polyfilled because on latest DXC pack_clamp_u8()
        // receives an int32_t4 as its input.
        // See https://github.com/microsoft/DirectXShaderCompiler/issues/5091 for more details.
        core_polyfills.pack_4xu8_clamp = true;
        core_polyfills.pack_unpack_4x8 = options.polyfill_pack_unpack_4x8;
        // core_polyfills.precise_float_mod = true;
        // core_polyfills.reflect_vec2_f32 = options.polyfill_reflect_vec2_f32;
        core_polyfills.texture_sample_base_clamp_to_edge_2d_f32 = true;
        // core_polyfills.workgroup_uniform_load = true;
        RUN_TRANSFORM(core::ir::transform::BuiltinPolyfill, core_polyfills);
    }

    {
        core::ir::transform::ConversionPolyfillConfig conversion_polyfills{};
        conversion_polyfills.ftoi = true;
        RUN_TRANSFORM(core::ir::transform::ConversionPolyfill, conversion_polyfills);
    }

    RUN_TRANSFORM(core::ir::transform::AddEmptyEntryPoint);

    RUN_TRANSFORM(raise::DecomposeMemoryAccess);

    if (options.compiler == Options::Compiler::kFXC) {
        RUN_TRANSFORM(raise::FxcPolyfill);
    }

    if (!options.disable_robustness) {
        core::ir::transform::RobustnessConfig config{};
        config.bindings_ignored = std::unordered_set<BindingPoint>(
            options.bindings.ignored_by_robustness_transform.cbegin(),
            options.bindings.ignored_by_robustness_transform.cend());

        // Direct3D guarantees to return zero for any resource that is accessed out of bounds, and
        // according to the description of the assembly store_uav_typed, out of bounds addressing
        // means nothing gets written to memory.
        //
        // TODO(dsinclair): Need to translate this into new robustness.
        // config.texture_action = ast::transform::Robustness::Action::kIgnore;

        RUN_TRANSFORM(core::ir::transform::Robustness, config);
    }
    if (!options.disable_workgroup_init) {
        RUN_TRANSFORM(core::ir::transform::ZeroInitWorkgroupMemory);
    }

    RUN_TRANSFORM(raise::ShaderIO);
    RUN_TRANSFORM(raise::BuiltinPolyfill);
    RUN_TRANSFORM(core::ir::transform::VectorizeScalarMatrixConstructors);

    // DemoteToHelper must come before any transform that introduces non-core instructions.
    RUN_TRANSFORM(core::ir::transform::DemoteToHelper);

    // These transforms need to be run last as various transforms introduce terminator arguments,
    // naming conflicts, and expressions that need to be explicitly not inlined.
    RUN_TRANSFORM(core::ir::transform::RemoveTerminatorArgs);
    RUN_TRANSFORM(core::ir::transform::RenameConflicts);
    RUN_TRANSFORM(core::ir::transform::ValueToLet);

    // Anything which runs after this needs to handle `Capabilities::kAllowModuleScopedLets`
    RUN_TRANSFORM(raise::PromoteInitializers);

    return Success;
}

}  // namespace tint::hlsl::writer
