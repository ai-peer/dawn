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

#include "src/tint/transform/vectorize_matrix_conversions.h"

#include <string>
#include <utility>

#include "src/tint/transform/test_helper.h"
#include "src/tint/utils/string.h"

namespace tint::transform {
namespace {

using VectorizeMatrixConversionsTest = TransformTestWithParam<std::pair<uint32_t, uint32_t>>;

TEST_F(VectorizeMatrixConversionsTest, ShouldRunEmptyModule) {
    auto* src = R"()";

    EXPECT_FALSE(ShouldRun<VectorizeMatrixConversions>(src));
}

TEST_P(VectorizeMatrixConversionsTest, Conversion_F32ToF16) {
    uint32_t cols = GetParam().first;
    uint32_t rows = GetParam().second;
    std::string src_mat_type = "mat" + std::to_string(cols) + "x" + std::to_string(rows) + "<f32>";
    std::string src_vec_type = "vec" + std::to_string(rows) + "<f32>";
    std::string dst_mat_type = "mat" + std::to_string(cols) + "x" + std::to_string(rows) + "<f16>";
    std::string dst_vec_type = "vec" + std::to_string(rows) + "<f16>";
    std::string vector_values;
    for (uint32_t c = 0; c < cols; c++) {
        if (c > 0) {
            vector_values += ", ";
        }
        vector_values += src_vec_type + "(";
        for (uint32_t r = 0; r < rows; r++) {
            if (r > 0) {
                vector_values += ", ";
            }
            auto value = std::to_string(c * rows + r) + ".0";
            vector_values += value;
        }
        vector_values += ")";
    }

    std::string vectorized_args = "";
    for (uint32_t c = 0; c < cols; c++) {
        if (c > 0) {
            vectorized_args += ", ";
        }
        vectorized_args += dst_vec_type + "(m[" + std::to_string(c) + "])";
    }

    std::string tmpl = R"(
enable f16;

@fragment
fn main() {
  let m = ${src_mat_type}(${values});
  let n : ${dst_mat_type} = ${dst_mat_type}(${args});
}
)";
    tmpl = utils::ReplaceAll(tmpl, "${src_mat_type}", src_mat_type);
    tmpl = utils::ReplaceAll(tmpl, "${dst_mat_type}", dst_mat_type);
    tmpl = utils::ReplaceAll(tmpl, "${values}", vector_values);
    auto src = utils::ReplaceAll(tmpl, "${args}", "m");
    auto expect = utils::ReplaceAll(tmpl, "${args}", vectorized_args);

    EXPECT_TRUE(ShouldRun<VectorizeMatrixConversions>(src));

    auto got = Run<VectorizeMatrixConversions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(VectorizeMatrixConversionsTest, Conversion_F16ToF32) {
    uint32_t cols = GetParam().first;
    uint32_t rows = GetParam().second;
    std::string src_mat_type = "mat" + std::to_string(cols) + "x" + std::to_string(rows) + "<f16>";
    std::string src_vec_type = "vec" + std::to_string(rows) + "<f16>";
    std::string dst_mat_type = "mat" + std::to_string(cols) + "x" + std::to_string(rows) + "<f32>";
    std::string dst_vec_type = "vec" + std::to_string(rows) + "<f32>";
    std::string vector_values;
    for (uint32_t c = 0; c < cols; c++) {
        if (c > 0) {
            vector_values += ", ";
        }
        vector_values += src_vec_type + "(";
        for (uint32_t r = 0; r < rows; r++) {
            if (r > 0) {
                vector_values += ", ";
            }
            auto value = std::to_string(c * rows + r) + ".0";
            vector_values += value;
        }
        vector_values += ")";
    }

    std::string vectorized_args = "";
    for (uint32_t c = 0; c < cols; c++) {
        if (c > 0) {
            vectorized_args += ", ";
        }
        vectorized_args += dst_vec_type + "(m[" + std::to_string(c) + "])";
    }

    std::string tmpl = R"(
enable f16;

@fragment
fn main() {
  let m = ${src_mat_type}(${values});
  let n : ${dst_mat_type} = ${dst_mat_type}(${args});
}
)";
    tmpl = utils::ReplaceAll(tmpl, "${src_mat_type}", src_mat_type);
    tmpl = utils::ReplaceAll(tmpl, "${dst_mat_type}", dst_mat_type);
    tmpl = utils::ReplaceAll(tmpl, "${values}", vector_values);
    auto src = utils::ReplaceAll(tmpl, "${args}", "m");
    auto expect = utils::ReplaceAll(tmpl, "${args}", vectorized_args);

    EXPECT_TRUE(ShouldRun<VectorizeMatrixConversions>(src));

    auto got = Run<VectorizeMatrixConversions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(VectorizeMatrixConversionsTest, NonConversion_ConstructorFromVectors) {
    uint32_t cols = GetParam().first;
    uint32_t rows = GetParam().second;
    std::string mat_type = "mat" + std::to_string(cols) + "x" + std::to_string(rows) + "<f32>";
    std::string vec_type = "vec" + std::to_string(rows) + "<f32>";
    std::string columns;
    for (uint32_t c = 0; c < cols; c++) {
        if (c > 0) {
            columns += ", ";
        }
        columns += vec_type + "()";
    }

    std::string tmpl = R"(
@fragment
fn main() {
  let m = ${matrix}(${columns});
}
)";
    tmpl = utils::ReplaceAll(tmpl, "${matrix}", mat_type);
    auto src = utils::ReplaceAll(tmpl, "${columns}", columns);
    auto expect = src;

    EXPECT_FALSE(ShouldRun<VectorizeMatrixConversions>(src));

    auto got = Run<VectorizeMatrixConversions>(src);

    EXPECT_EQ(expect, str(got));
}

TEST_P(VectorizeMatrixConversionsTest, NonConversion_IdentityConstructor) {
    uint32_t cols = GetParam().first;
    uint32_t rows = GetParam().second;
    std::string mat_type = "mat" + std::to_string(cols) + "x" + std::to_string(rows) + "<f32>";
    std::string vec_type = "vec" + std::to_string(rows) + "<f32>";
    std::string columns;
    for (uint32_t c = 0; c < cols; c++) {
        if (c > 0) {
            columns += ", ";
        }
        columns += vec_type + "()";
    }

    std::string tmpl = R"(
@fragment
fn main() {
  let m = ${matrix}(${columns});
  let n : ${matrix} = ${matrix}(m);
}
)";
    tmpl = utils::ReplaceAll(tmpl, "${matrix}", mat_type);
    auto src = utils::ReplaceAll(tmpl, "${columns}", columns);
    auto expect = src;

    EXPECT_FALSE(ShouldRun<VectorizeMatrixConversions>(src));

    auto got = Run<VectorizeMatrixConversions>(src);

    EXPECT_EQ(expect, str(got));
}

INSTANTIATE_TEST_SUITE_P(VectorizeMatrixConversionsTest,
                         VectorizeMatrixConversionsTest,
                         testing::Values(std::make_pair(2, 2),
                                         std::make_pair(2, 3),
                                         std::make_pair(2, 4),
                                         std::make_pair(3, 2),
                                         std::make_pair(3, 3),
                                         std::make_pair(3, 4),
                                         std::make_pair(4, 2),
                                         std::make_pair(4, 3),
                                         std::make_pair(4, 4)));

}  // namespace
}  // namespace tint::transform
