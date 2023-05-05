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

#include "src/tint/ir/builder_impl.h"
#include "src/tint/ir/test_helper.h"
#include "src/tint/ir/to_program.h"
#include "src/tint/reader/wgsl/parser.h"
#include "src/tint/utils/string.h"
#include "src/tint/writer/wgsl/generator.h"

namespace tint::ir {
namespace {

using namespace tint::number_suffixes;  // NOLINT

struct Case {
    std::string_view in;
    std::string_view out = "";
};
std::ostream& operator<<(std::ostream& out, const Case& c) {
    return out << "input:" << std::endl << c.in;
}

using IRToProgramRoundtripTest = TestParamHelper<Case>;

TEST_P(IRToProgramRoundtripTest, Test) {
    auto input_wgsl = utils::TrimSpace(GetParam().in);
    Source::File file("test.wgsl", std::string(input_wgsl));
    auto input_program = reader::wgsl::Parse(&file);
    ASSERT_TRUE(input_program.IsValid()) << input_program.Diagnostics().str();
    ir::BuilderImpl builder(&input_program);
    auto ir_module = builder.Build();
    ASSERT_TRUE(ir_module);
    auto output_program = ToProgram(ir_module.Get());
    ASSERT_TRUE(output_program.IsValid()) << output_program.Diagnostics().str();
    auto output = writer::wgsl::Generate(&output_program, {});
    ASSERT_TRUE(output.success) << output.error;
    auto expected_wgsl = GetParam().out.empty() ? input_wgsl : utils::TrimSpace(GetParam().out);
    auto got_wgsl = utils::TrimSpace(output.wgsl);
    if (expected_wgsl != got_wgsl) {
        tint::ir::Disassembler d{ir_module.Get()};
        EXPECT_EQ(expected_wgsl, got_wgsl) << "IR:" << std::endl << d.Disassemble();
    }
}

INSTANTIATE_TEST_SUITE_P(,
                         IRToProgramRoundtripTest,
                         testing::ValuesIn(std::vector<Case>{
                             {R"()"},
                             {R"(
fn f() {
}
)"},
                             {R"(
fn f() {
  var i : i32 = 42i;
}
)"},
                         }));

}  // namespace
}  // namespace tint::ir
