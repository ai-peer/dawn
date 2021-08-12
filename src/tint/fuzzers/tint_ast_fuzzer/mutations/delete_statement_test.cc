// Copyright 2021 The Tint Authors.
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

#include "src/tint/fuzzers/tint_ast_fuzzer/mutations/delete_statement.h"

#include <string>

#include "gtest/gtest.h"

#include "src/tint/fuzzers/tint_ast_fuzzer/mutator.h"
#include "src/tint/fuzzers/tint_ast_fuzzer/node_id_map.h"
#include "src/tint/fuzzers/tint_ast_fuzzer/probability_context.h"
#include "src/tint/program_builder.h"
#include "src/tint/reader/wgsl/parser.h"
#include "src/tint/writer/wgsl/generator.h"

namespace tint::fuzzers::ast_fuzzer {
namespace {

TEST(DeleteStatementTest, Applicable1) {
  std::string content = R"(
    fn main() {
      {
        var a: i32 = 5;
        a = 6;
      }
    }
  )";
  Source::File file("test.wgsl", content);
  auto program = reader::wgsl::Parse(&file);
  ASSERT_TRUE(program.IsValid()) << program.Diagnostics().str();

  NodeIdMap node_id_map(program);

  const auto& main_fn_statements =
      program.AST().Functions()[0]->body->statements;

  auto* statement = main_fn_statements[0]
                        ->As<ast::BlockStatement>()
                        ->statements[1]
                        ->As<ast::AssignmentStatement>();
  ASSERT_NE(statement, nullptr);

  auto statement_id = node_id_map.GetId(statement);
  ASSERT_NE(statement_id, 0);

  // Assignment statement can be deleted.
  ASSERT_TRUE(MaybeApplyMutation(program, MutationDeleteStatement(statement_id),
                                 node_id_map, &program, &node_id_map, nullptr));
  ASSERT_TRUE(program.IsValid()) << program.Diagnostics().str();
  //
  //  writer::wgsl::Options options;
  //  auto result = writer::wgsl::Generate(&program, options);
  //  ASSERT_TRUE(result.success) << result.error;
  //
  //  std::string expected_shader = R"(fn main() {
  //  var a : i32 = 5;
  //}
  //)";
  //  ASSERT_EQ(expected_shader, result.wgsl);
}

}  // namespace
}  // namespace tint::fuzzers::ast_fuzzer
