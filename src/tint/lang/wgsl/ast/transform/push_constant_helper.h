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

#ifndef SRC_TINT_LANG_WGSL_AST_TRANSFORM_PUSH_CONSTANT_HELPER_H_
#define SRC_TINT_LANG_WGSL_AST_TRANSFORM_PUSH_CONSTANT_HELPER_H_

#include <map>

#include "src/tint/lang/wgsl/program/program_builder.h"

namespace tint::program {
class CloneContext;
}
namespace tint::ast {
struct Type;
class StructMember;
}  // namespace tint::ast
namespace tint::ast::transform {

/// A helper that manages the finding, reading, and modifying of push_constant blocks.
/// This is used by transforms that wish to add new data to the single push_constant block
/// which is allowed per entry point.
class PushConstantHelper {
  public:
    /// Constructor
    explicit PushConstantHelper(program::CloneContext& c);
    ~PushConstantHelper();

    void InsertMember(const char* name, ast::Type type, uint32_t offset);
    Symbol Run();

  private:
    std::map<uint32_t, const tint::ast::StructMember*> member_map;
    program::CloneContext& ctx;
    const ast::Struct* new_struct = nullptr;
    const ast::Variable* push_constants_var = nullptr;
};

}  // namespace tint::ast::transform

#endif  // SRC_TINT_LANG_WGSL_AST_TRANSFORM_PUSH_CONSTANT_HELPER_H_
