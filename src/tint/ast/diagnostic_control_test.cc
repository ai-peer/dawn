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

#include "src/tint/ast/diagnostic_control.h"

#include "src/tint/ast/test_helper.h"

namespace tint::ast {
namespace {

using DiagnosticControlTest = TestHelper;

TEST_F(DiagnosticControlTest, Creation) {
    auto* name = Expr("foo");
    auto* control = create<ast::DiagnosticControl>(Source{{{20, 2}, {20, 5}}},
                                                   DiagnosticSeverity::kWarning, name);
    EXPECT_EQ(control->source.range.begin.line, 20u);
    EXPECT_EQ(control->source.range.begin.column, 2u);
    EXPECT_EQ(control->source.range.end.line, 20u);
    EXPECT_EQ(control->source.range.end.column, 5u);
    EXPECT_EQ(control->severity, DiagnosticSeverity::kWarning);
    EXPECT_EQ(control->rule_name, name);
}

namespace parse_print_tests {

namespace severity {

struct Case {
    const char* string;
    DiagnosticSeverity value;
};

static constexpr Case kValidCases[] = {
    {"error", DiagnosticSeverity::kError},
    {"warning", DiagnosticSeverity::kWarning},
    {"info", DiagnosticSeverity::kInfo},
    {"off", DiagnosticSeverity::kOff},
};

static constexpr Case kInvalidCases[] = {
    {"3", DiagnosticSeverity::kUndefined},    {"errorr", DiagnosticSeverity::kUndefined},
    {"0ff", DiagnosticSeverity::kUndefined},  {"Info", DiagnosticSeverity::kUndefined},
    {"note", DiagnosticSeverity::kUndefined}, {"waring", DiagnosticSeverity::kUndefined},
};

using DiagnosticSeverityParseTest = testing::TestWithParam<Case>;

TEST_P(DiagnosticSeverityParseTest, Parse) {
    const char* string = GetParam().string;
    DiagnosticSeverity expect = GetParam().value;
    EXPECT_EQ(expect, ParseDiagnosticSeverity(string));
}

INSTANTIATE_TEST_SUITE_P(ValidCases, DiagnosticSeverityParseTest, testing::ValuesIn(kValidCases));
INSTANTIATE_TEST_SUITE_P(InvalidCases,
                         DiagnosticSeverityParseTest,
                         testing::ValuesIn(kInvalidCases));

using DiagnosticSeverityPrintTest = testing::TestWithParam<Case>;

TEST_P(DiagnosticSeverityPrintTest, Print) {
    DiagnosticSeverity value = GetParam().value;
    const char* expect = GetParam().string;
    EXPECT_EQ(expect, utils::ToString(value));
}

INSTANTIATE_TEST_SUITE_P(ValidCases, DiagnosticSeverityPrintTest, testing::ValuesIn(kValidCases));

}  // namespace severity

}  // namespace parse_print_tests

}  // namespace
}  // namespace tint::ast
