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

#ifndef SRC_TINT_LANG_WGSL_AST_TRANSFORM_OFFSET_FIRST_INDEX_H_
#define SRC_TINT_LANG_WGSL_AST_TRANSFORM_OFFSET_FIRST_INDEX_H_

#include "src/tint/lang/wgsl/ast/transform/transform.h"

namespace tint::ast::transform {

/// Adds firstVertex/Instance (injected via root constants) to
/// vertex/instance index builtins.
///
/// This transform assumes that Name transform has been run before.
///
/// Unlike other APIs, D3D always starts vertex and instance numbering at 0,
/// regardless of the firstVertex/Instance value specified. This transformer
/// adds the value of firstVertex/Instance to each builtin. This action is
/// performed by adding a new constant equal to original builtin +
/// firstVertex/Instance to each function that references one of these builtins.
///
/// Note that D3D does not have any semantics for firstVertex/Instance.
/// Therefore, these values must by passed to the shader.
///
/// Before:
/// ```
///   @builtin(vertex_index) var<in> vert_idx : u32;
///   fn func() -> u32 {
///     return vert_idx;
///   }
/// ```
///
/// After:
/// ```
///   @location(N) var<push_constant> tint_first_vertex_index : u32;
///   @location(M) var<push_constant> tint_first_instance_index : u32;
///   @builtin(vertex_index) var<in> tint_first_index_offset_vert_idx : u32;
///   fn func() -> u32 {
///     const vert_idx = (tint_first_index_offset_vert_idx +
///                       tint_first_vertex_index);
///     return vert_idx;
///   }
/// ```
///
class OffsetFirstIndex final : public Castable<OffsetFirstIndex, Transform> {
  public:
    /// Data is output by the OffsetFirstIndex transform.
    /// Data holds information about shader usage and constant buffer offsets.
    struct Data final : public Castable<Data, transform::Data> {
        /// Constructor
        /// @param has_vtx_index True if the shader uses vertex_index
        /// @param has_inst_index True if the shader uses instance_index
        Data(bool has_vtx_index, bool has_inst_index);

        /// Copy constructor
        Data(const Data&);

        /// Destructor
        ~Data() override;

        /// True if the shader uses vertex_index
        const bool has_vertex_index;
        /// True if the shader uses instance_index
        const bool has_instance_index;
    };

    /// Transform configuration options
    struct Config final : public Castable<Config, ast::transform::Data> {
        /// Constructor
        /// @param first_vertex_loc Location of the firstVertex push constant
        /// @param first_instance_loc Location of the firstInstance push constant
        Config(int32_t first_vertex_loc, int32_t first_instance_loc);

        /// Destructor
        ~Config() override;

        /// Location of the firstVertex push constant
        int32_t first_vertex_location;

        /// Location of the firstInstance push constant
        int32_t first_instance_location;
    };

    /// Constructor
    OffsetFirstIndex();
    /// Destructor
    ~OffsetFirstIndex() override;

    /// @copydoc Transform::Apply
    ApplyResult Apply(const Program& program,
                      const DataMap& inputs,
                      DataMap& outputs) const override;
};

}  // namespace tint::ast::transform

#endif  // SRC_TINT_LANG_WGSL_AST_TRANSFORM_OFFSET_FIRST_INDEX_H_
