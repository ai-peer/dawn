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

#include "gtest/gtest.h"

#include "tint/interp/shader_executor.h"
#include "tint/tint.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {
namespace {

class ShaderExecutorTest : public testing::Test {
  protected:
    // Initialize the test with a WGSL source string.
    void Init(std::string source) {
        // Parse the source file to produce a Tint program object.
        file_ = std::make_unique<Source::File>("test.wgsl", source);
        program_ = std::make_unique<Program>(wgsl::reader::Parse(file_.get()));
        auto diag_printer = StyledTextPrinter::Create(stderr);
        diag::Formatter diag_formatter;
        ASSERT_TRUE(program_) << "Failed to run WGSL parser.";
        if (program_->Diagnostics().Count() > 0) {
            diag_printer->Print(diag_formatter.Format(program_->Diagnostics()));
        }
        ASSERT_TRUE(program_->IsValid()) << "Source WGSL was invalid.";
    }

    std::unique_ptr<Source::File> file_;
    std::unique_ptr<Program> program_;
};

TEST_F(ShaderExecutorTest, EntryPointNotFound) {
    Init(R"(
@compute @workgroup_size(1)
fn foo() {
})");
    auto result = ShaderExecutor::Create(*program_, "main", {});
    EXPECT_NE(result, Success);
    EXPECT_EQ(result.Failure(), R"(error: entry point 'main' not found in module)");
}

TEST_F(ShaderExecutorTest, EntryPointNotCompute) {
    Init(R"(
@fragment
fn main() {
})");
    auto result = ShaderExecutor::Create(*program_, "main", {});
    EXPECT_NE(result, Success);
    EXPECT_EQ(result.Failure(), R"(error: function 'main' is not a compute shader)");
}

TEST_F(ShaderExecutorTest, MissingNamedOverride) {
    Init(R"(
override x : i32;

@compute @workgroup_size(1)
fn main() {
    let y = x;
})");
    auto result = ShaderExecutor::Create(*program_, "main", {});
    EXPECT_NE(result, Success);
    EXPECT_EQ(result.Failure(), R"(test.wgsl:2:10 error: missing pipeline-override value for 'x'
override x : i32;
         ^
)");
}

TEST_F(ShaderExecutorTest, MissingBufferBinding) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> x : i32;

@compute @workgroup_size(1)
fn main() {
    x++;
})");
    auto executor = ShaderExecutor::Create(*program_, "main", {});
    ASSERT_EQ(executor, Success) << executor.Failure();
    auto result = executor.Get()->Run({1, 1, 1}, {});
    EXPECT_EQ(result.Failure(), R"(error: missing buffer binding for @group(0) @binding(0))");
}

TEST_F(ShaderExecutorTest, InvalidBufferBinding) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> x : i32;

@compute @workgroup_size(1)
fn main() {
    x++;
})");
    auto executor = ShaderExecutor::Create(*program_, "main", {});
    ASSERT_EQ(executor, Success) << executor.Failure();

    Binding buffer;
    buffer.buffer = nullptr;
    BindingList bindings;
    bindings[{0, 0}] = buffer;
    auto result = executor.Get()->Run({1, 1, 1}, std::move(bindings));
    EXPECT_EQ(result.Failure(), R"(error: invalid binding resource for @group(0) @binding(0))");
}

}  // namespace
}  // namespace tint::interp
