
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

#include "src/tint/lang/msl/writer/raise/module_scope_var_to_entry_point_param.h"
#include "src/tint/lang/core/ir/transform/helper_test.h"

namespace tint::msl::writer::raise {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using MslPrinterTest_ModScopeVarToEP = core::ir::transform::TransformTest;

TEST_F(MslPrinterTest_ModScopeVarToEP, Basic) {
    core::ir::Instruction* p = nullptr;
    core::ir::Instruction* w = nullptr;

    b.Append(mod.root_block, [&] {
        p = b.Var<private_, f32>("p");
        w = b.Var<workgroup, f32>("w");
    });

    auto* ep = b.Function("main", ty.void_());
    ep->SetStage(core::ir::Function::PipelineStage::kCompute);
    b.Append(ep->Block(), [&] {  //
        auto* l = b.Load(p);
        b.Store(w, l);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %p:ptr<private, f32, read_write> = var
  %w:ptr<workgroup, f32, read_write> = var
}

%main = @compute func():void -> %b2 {
  %b2 = block {
    %4:f32 = load %p
    store %w, %4
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
struct tint_private_vars {
  p: f32,
}
%main = @compute func():void -> %b1 {
  %b1 = block {
    %p:ptr<private, tint_private_vars, read_write> = var
    %w:ptr<workgroup, f32, read_write> = var
    %2:f32 = access %p 0
    store %w, %2
    ret
  }
}
)";

    Run(ModuleScopeVarToEntryPointParam);

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::msl::writer::raise
