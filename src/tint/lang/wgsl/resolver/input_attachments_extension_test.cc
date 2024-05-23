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

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#if TINT_BUILD_WGSL_READER
#include "src/tint/lang/wgsl/reader/reader.h"
#endif
#include "src/tint/lang/wgsl/resolver/resolve.h"
#include "src/tint/lang/wgsl/resolver/resolver.h"
#include "src/tint/lang/wgsl/resolver/resolver_helper_test.h"

namespace tint::resolver {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

void ParseSource([[maybe_unused]] std::string src, [[maybe_unused]] bool should_pass) {
#if TINT_BUILD_WGSL_READER
    wgsl::reader::Options options;
    options.allowed_features = wgsl::AllowedFeatures::Everything();
    auto file = std::make_unique<Source::File>("test", src);
    auto program = wgsl::reader::Parse(file.get(), options);

    auto error = program.Diagnostics().Str();

    bool valid = program.IsValid();
    if (should_pass) {
        EXPECT_TRUE(valid) << error;
        EXPECT_FALSE(program.Diagnostics().ContainsErrors());
    } else {
        EXPECT_FALSE(valid);
    }
#endif
}

using InputAttachmenExtensionTest = ResolverTest;

// Test that Resolver can get input_attachment_index value.
TEST_F(InputAttachmenExtensionTest, InputAttachmentIndex) {
    // enable chromium_internal_input_attachments;
    // @group(0) @binding(0) @input_attachment_index(3)
    // var input_tex : input_attachment<f32>;

    Enable(Source{{12, 34}}, wgsl::Extension::kChromiumInternalInputAttachments);

    auto* ast_var = GlobalVar("input_tex", ty.input_attachment(ty.Of<f32>()),
                              Vector{Binding(0_u), Group(0_u), InputAttachmentIndex(3_u)});

    EXPECT_TRUE(r()->Resolve()) << r()->error();

    auto* sem_var = Sem().Get<sem::GlobalVariable>(ast_var);
    ASSERT_NE(sem_var, nullptr);
    EXPECT_EQ(sem_var->Attributes().input_attachment_index, 3u);
}

// Test that input_attachment_index's value cannot be float.
TEST_F(InputAttachmenExtensionTest, InputAttachmentIndexInvalidType) {
    // enable chromium_internal_input_attachments;
    // @group(0) @binding(0) @input_attachment_index(3.0)
    // var input_tex : input_attachment<f32>;

    Enable(Source{{12, 34}}, wgsl::Extension::kChromiumInternalInputAttachments);

    GlobalVar("input_tex", ty.input_attachment(ty.Of<f32>()),
              Vector{Binding(0_u), Group(0_u), InputAttachmentIndex(3_f)});

    EXPECT_FALSE(r()->Resolve());
    EXPECT_EQ(r()->error(), R"(error: '@input_attachment_index' must be an 'i32' or 'u32' value)");
}

// Test that Parser can parse input_attachment_index with integer value.
TEST_F(InputAttachmenExtensionTest, InputAttachmentIndexFromSource) {
    ParseSource(R"(
enable chromium_internal_input_attachments;
@group(0) @binding(0) @input_attachment_index(3)
var input_tex : input_attachment<f32>;
    )",
                /*should_pass=*/true);
}

// Test that input_attachment_index from source cannot have float value.
TEST_F(InputAttachmenExtensionTest, InputAttachmentIndexInvalidTypeFromSource) {
    ParseSource(R"(
enable chromium_internal_input_attachments;
@group(0) @binding(0) @input_attachment_index(3.0)
var input_tex : input_attachment<f32>;
    )",
                /*should_pass=*/false);
}

}  // namespace
}  // namespace tint::resolver
