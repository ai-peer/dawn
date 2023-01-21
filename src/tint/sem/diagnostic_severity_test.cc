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

#include "src/tint/sem/test_helper.h"

#include "src/tint/sem/module.h"

using namespace tint::number_suffixes;  // NOLINT

namespace tint::sem {
namespace {

class DiagnosticSeverityTest : public TestHelper {
  protected:
    void Run(ast::DiagnosticSeverity global_severity) {
        // @diagnostic(off, chromium_unreachable_code)
        // fn foo() {
        //   return;
        // }
        //
        // fn bar() {
        //   return;
        // }
        auto* return_1 = Return();
        auto* return_2 = Return();
        auto* attr =
            DiagnosticAttribute(ast::DiagnosticSeverity::kOff, Expr("chromium_unreachable_code"));
        auto* foo = Func("foo", {}, ty.void_(), utils::Vector{return_1}, utils::Vector{attr});
        auto* bar = Func("bar", {}, ty.void_(), utils::Vector{return_2});

        auto p = Build();
        EXPECT_TRUE(p.IsValid()) << p.Diagnostics().str();

        EXPECT_EQ(p.Sem().DiagnosticSeverity(foo, ast::DiagnosticRule::kChromiumUnreachableCode),
                  ast::DiagnosticSeverity::kOff);
        EXPECT_EQ(
            p.Sem().DiagnosticSeverity(return_1, ast::DiagnosticRule::kChromiumUnreachableCode),
            ast::DiagnosticSeverity::kOff);
        EXPECT_EQ(p.Sem().DiagnosticSeverity(bar, ast::DiagnosticRule::kChromiumUnreachableCode),
                  global_severity);
        EXPECT_EQ(
            p.Sem().DiagnosticSeverity(return_2, ast::DiagnosticRule::kChromiumUnreachableCode),
            global_severity);
    }
};

TEST_F(DiagnosticSeverityTest, WithDirective) {
    DiagnosticDirective(ast::DiagnosticSeverity::kError, Expr("chromium_unreachable_code"));
    Run(ast::DiagnosticSeverity::kError);
}

TEST_F(DiagnosticSeverityTest, WithoutDirective) {
    Run(ast::DiagnosticSeverity::kWarning);
}

}  // namespace
}  // namespace tint::sem
