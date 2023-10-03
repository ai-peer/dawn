// Copyright 2020 The Tint Authors.
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

// GEN_BUILD:CONDITION(tint_build_wgsl_reader && tint_build_wgsl_writer)

#include <string>
#include <unordered_set>

#include "src/tint/cmd/fuzz/wgsl/wgsl_fuzz.h"
#include "src/tint/lang/wgsl/reader/parser/parser.h"
#include "src/tint/lang/wgsl/writer/writer.h"

namespace tint::program {

#define ASSERT_EQ(A, B)                                         \
    do {                                                        \
        decltype(A) assert_a = (A);                             \
        decltype(B) assert_b = (B);                             \
        if (assert_a != assert_b) {                             \
            TINT_ICE() << "ASSERT_EQ(" #A ", " #B ") failed:\n" \
                       << #A << " was: " << assert_a << "\n"    \
                       << #B << " was: " << assert_b << "\n";   \
        }                                                       \
    } while (false)

#define ASSERT_TRUE(A)                                                                           \
    do {                                                                                         \
        decltype(A) assert_a = (A);                                                              \
        if (!assert_a) {                                                                         \
            TINT_ICE() << "ASSERT_TRUE(" #A ") failed:\n" << #A << " was: " << assert_a << "\n"; \
        }                                                                                        \
    } while (false)

void CloneContextFuzzer(const tint::Program& src) {
    // Clone the src program to dst
    tint::Program dst(src.Clone());

    // Expect the printed strings to match
    ASSERT_EQ(tint::Program::printer(src), tint::Program::printer(dst));

    // Check that none of the AST nodes or type pointers in dst are found in src
    std::unordered_set<const tint::ast::Node*> src_nodes;
    for (auto* src_node : src.ASTNodes().Objects()) {
        src_nodes.emplace(src_node);
    }
    std::unordered_set<const tint::core::type::Type*> src_types;
    for (auto* src_type : src.Types()) {
        src_types.emplace(src_type);
    }
    for (auto* dst_node : dst.ASTNodes().Objects()) {
        ASSERT_EQ(src_nodes.count(dst_node), 0u);
    }
    for (auto* dst_type : dst.Types()) {
        ASSERT_EQ(src_types.count(dst_type), 0u);
    }

    tint::wgsl::writer::Options wgsl_options;

    auto src_wgsl = tint::wgsl::writer::Generate(src, wgsl_options);
    ASSERT_TRUE(src_wgsl);

    auto dst_wgsl = tint::wgsl::writer::Generate(dst, wgsl_options);
    ASSERT_TRUE(dst_wgsl);

    ASSERT_EQ(src_wgsl->wgsl, dst_wgsl->wgsl);
}

}  // namespace tint::program

TINT_WGSL_PROGRAM_FUZZER(tint::program::CloneContextFuzzer);
