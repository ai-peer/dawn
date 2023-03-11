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

#include <array>
#include <functional>

#include "gtest/gtest.h"
#include "tint/interp/memory.h"
#include "tint/interp/shader_executor.h"
#include "tint/tint.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {
namespace {

class ComputeEndToEndTestBase {
  protected:
    // Initialize the test with a WGSL source string.
    void Init(std::string source, NamedOverrideList&& overrides = {}) {
        // Parse the source file to produce a Tint program object.
        wgsl::reader::Options options;
        options.allowed_features = wgsl::AllowedFeatures::Everything();
        file_ = std::make_unique<Source::File>("test.wgsl", source);
        program_ = std::make_unique<Program>(wgsl::reader::Parse(file_.get(), options));
        auto diag_printer = StyledTextPrinter::Create(stderr);
        diag::Formatter diag_formatter;
        ASSERT_TRUE(program_) << "Failed to run WGSL parser.";
        if (program_->Diagnostics().Count() > 0) {
            diag_printer->Print(diag_formatter.Format(program_->Diagnostics()));
        }
        ASSERT_TRUE(program_->IsValid()) << "Source WGSL was invalid.";

        // Create the interpreter.
        auto result = ShaderExecutor::Create(*program_, "main", std::move(overrides));
        ASSERT_EQ(result, Success) << result.Failure();
        executor_ = result.Move();
        executor_->AddErrorCallback([&](auto&& error) { errors_ += error.Str() + "\n"; });
    }

    // Run the shader.
    // Returns `false` if the execution fails or errors are present, otherwise `true`.
    bool RunShader(UVec3 group_count, BindingList&& bindings, bool expect_errors = false) {
        if (executor_->Run(group_count, std::move(bindings)) != Success) {
            return false;
        }
        if (expect_errors) {
            if (errors_.empty()) {
                std::cerr << "errors expected, but none were generated" << std::endl;
                return false;
            }
        } else {
            if (!errors_.empty()) {
                std::cerr << errors_ << std::endl;
                return false;
            }
        }
        return true;
    }

    // Helper to create a buffer from a set of values.
    template <typename T, unsigned N = 1>
    std::unique_ptr<Memory> MakeBuffer(std::array<T, N> values) {
        auto buffer = std::make_unique<Memory>(N * sizeof(T));
        for (size_t i = 0; i < N; i++) {
            buffer->Store(&values[i], i * sizeof(T));
        }
        return buffer;
    }

    // Helper to create a zero-initialized buffer with a given size.
    std::unique_ptr<Memory> MakeZeroInitBuffer(size_t size) {
        return std::make_unique<Memory>(size);
    }

    // Helper to check if the contents of `buffer` at `offset` is equal to `value`.
    template <typename T>
    bool CheckEqual(Memory& buffer, uint64_t offset, T value) {
        T result;
        buffer.Load(&result, offset);
        if constexpr (std::is_floating_point_v<T>) {
            // We don't need to be particularly precise for the kinds of tests done here.
            T diff = std::abs(result - value);
            EXPECT_LE(diff, 0.000001f);
            return diff < 0.000001f;
        } else {
            EXPECT_EQ(result, value);
            return result == value;
        }
    }

    // Helper to check if the values in `buffer` are equal to `value`.
    template <typename T>
    bool CheckEqual(Memory& buffer, std::initializer_list<T> values) {
        uint64_t offset = 0;
        for (auto value : values) {
            if (!CheckEqual<T>(buffer, offset, value)) {
                return false;
            }
            offset += sizeof(T);
        }
        return true;
    }

    std::unique_ptr<Source::File> file_;
    std::unique_ptr<Program> program_;
    std::unique_ptr<ShaderExecutor> executor_;
    std::string errors_;
};

class ComputeEndToEndTest : protected ComputeEndToEndTestBase, public testing::Test {};

TEST_F(ComputeEndToEndTest, Basic) {
    Init(R"(
@compute @workgroup_size(1)
fn main() {
}
)");
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}));
}

TEST_F(ComputeEndToEndTest, StorageBuffer) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : i32;

@compute @workgroup_size(1)
fn main() {
  buffer = buffer + 10;
}
)");

    auto buffer = MakeBuffer<int32_t>({42});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 0, 52));
}

TEST_F(ComputeEndToEndTest, UniformBuffer) {
    Init(R"(
@group(0) @binding(0) var<uniform> input : i32;
@group(0) @binding(1) var<storage, read_write> output : i32;

@compute @workgroup_size(1)
fn main() {
  output = input + 10;
}
)");

    auto input = MakeBuffer<int32_t>({42});
    auto output = MakeBuffer<int32_t>({0});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(input.get(), 0, input->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, 0, 52));
}

TEST_F(ComputeEndToEndTest, BufferBindingOffset) {
    Init(R"(
@group(0) @binding(0) var<uniform> input : vec2<i32>;
@group(0) @binding(1) var<storage, read_write> output : vec2<i32>;

@compute @workgroup_size(1)
fn main() {
  output = input;
}
)");

    auto input = MakeBuffer<int32_t, 4>({99, 99, 42, -7});
    auto output = MakeBuffer<int32_t, 4>({-1, -1, -1, -1});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(input.get(), 8, 2 * sizeof(int32_t));
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 4, 2 * sizeof(int32_t));
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {-1, 42, -7, -1}));
}

TEST_F(ComputeEndToEndTest, RuntimeSizedArray) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<i32>;

@compute @workgroup_size(1)
fn main() {
  for (var i = 0; i < 8; i++) {
    buffer[i] = i;
  }
}
)");

    auto buffer = MakeBuffer<int32_t, 8>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    for (size_t i = 0; i < 8; i++) {
        EXPECT_TRUE(CheckEqual<int32_t>(*buffer, i * 4, static_cast<int32_t>(i)));
    }
}

TEST_F(ComputeEndToEndTest, RuntimeSizedArrayWithPadding) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<i32>>;

@compute @workgroup_size(1)
fn main() {
  for (var i = 0; i < 3; i++) {
    buffer[i].x = i * 3 + 0;
    buffer[i].y = i * 3 + 1;
    buffer[i].z = i * 3 + 2;
  }
}
)");

    auto buffer = MakeBuffer<int32_t, 12>({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    for (size_t i = 0; i < 3; i++) {
        EXPECT_TRUE(CheckEqual<int32_t>(*buffer, (i * 16) + 0, static_cast<int32_t>(i * 3 + 0)));
        EXPECT_TRUE(CheckEqual<int32_t>(*buffer, (i * 16) + 4, static_cast<int32_t>(i * 3 + 1)));
        EXPECT_TRUE(CheckEqual<int32_t>(*buffer, (i * 16) + 8, static_cast<int32_t>(i * 3 + 2)));
        EXPECT_TRUE(CheckEqual<int32_t>(*buffer, (i * 16) + 12, static_cast<int32_t>(0)));
    }
}

TEST_F(ComputeEndToEndTest, RuntimeSizedArrayInStruct) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
  arr : array<i32>,
}
@group(0) @binding(0) var<storage, read_write> buffer : S;

@compute @workgroup_size(1)
fn main() {
  for (var i = 0; i < 8; i++) {
    buffer.arr[i] = i * buffer.a + buffer.b;
  }
}
)");

    int32_t a = 2;
    int32_t b = 10;
    auto buffer = MakeBuffer<int32_t, 10>({a, b, 0, 0, 0, 0, 0, 0, 0, 0});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    for (size_t i = 0; i < 8; i++) {
        EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 8 + i * 4, static_cast<int32_t>(i) * a + b));
    }
}

TEST_F(ComputeEndToEndTest, ArrayLength) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> output : array<u32>;
@group(0) @binding(1) var<storage, read_write> buffer : array<vec3<u32>>;

@compute @workgroup_size(1)
fn main() {
  for (var i = 0u; i < arrayLength(&output); i++) {
    output[i] = arrayLength(&buffer);
  }
}
)");

    constexpr uint32_t size = 17;
    auto output = MakeZeroInitBuffer(size * sizeof(uint32_t));
    auto buffer = MakeBuffer<uint32_t, 16>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    ASSERT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    for (size_t i = 0; i < size; i++) {
        EXPECT_TRUE(CheckEqual<uint32_t>(*output, i * 4, 4));
    }
}

class ComputeBuiltinsTest : public ComputeEndToEndTest {
  protected:
    void Run(const char* wgsl) {
        Init(wgsl);

        constexpr uint32_t num_groups_x = 2;
        constexpr uint32_t num_groups_y = 3;
        constexpr uint32_t num_groups_z = 4;
        constexpr uint32_t wgsize_x = 4;
        constexpr uint32_t wgsize_y = 3;
        constexpr uint32_t wgsize_z = 2;
        constexpr uint32_t wgsize = wgsize_x * wgsize_y * wgsize_z;
        constexpr uint32_t total_invocations = wgsize * num_groups_x * num_groups_y * num_groups_z;
        auto local_id = MakeBuffer<uint32_t, total_invocations * 4>({});
        auto local_index = MakeBuffer<uint32_t, total_invocations>({});
        auto global_id = MakeBuffer<uint32_t, total_invocations * 4>({});
        auto group_id = MakeBuffer<uint32_t, total_invocations * 4>({});
        auto num_groups = MakeBuffer<uint32_t, total_invocations * 4>({});

        BindingList bindings;
        bindings[{0, 0}] = Binding::MakeBufferBinding(local_id.get(), 0, local_id->Size());
        bindings[{0, 1}] = Binding::MakeBufferBinding(local_index.get(), 0, local_index->Size());
        bindings[{0, 2}] = Binding::MakeBufferBinding(global_id.get(), 0, global_id->Size());
        bindings[{0, 3}] = Binding::MakeBufferBinding(group_id.get(), 0, group_id->Size());
        bindings[{0, 4}] = Binding::MakeBufferBinding(num_groups.get(), 0, num_groups->Size());
        ASSERT_EQ(
            executor_->Run(UVec3(num_groups_x, num_groups_y, num_groups_z), std::move(bindings)),
            Success);

        auto check = [&](uint32_t lx, uint32_t ly, uint32_t lz, uint32_t gx, uint32_t gy,
                         uint32_t gz) {
            auto local_idx = ((lz * wgsize_y) + ly) * wgsize_x + lx;
            auto group_idx = ((gz * num_groups_y) + gy) * num_groups_x + gx;
            auto global_id_x = gx * wgsize_x + lx;
            auto global_id_y = gy * wgsize_y + ly;
            auto global_id_z = gz * wgsize_z + lz;
            auto global_idx = group_idx * wgsize + local_idx;

            EXPECT_TRUE(CheckEqual<uint32_t>(*local_id, global_idx * 16 + 0, lx));
            EXPECT_TRUE(CheckEqual<uint32_t>(*local_id, global_idx * 16 + 4, ly));
            EXPECT_TRUE(CheckEqual<uint32_t>(*local_id, global_idx * 16 + 8, lz));

            EXPECT_TRUE(CheckEqual<uint32_t>(*local_index, global_idx * 4, local_idx));

            EXPECT_TRUE(CheckEqual<uint32_t>(*global_id, global_idx * 16 + 0, global_id_x));
            EXPECT_TRUE(CheckEqual<uint32_t>(*global_id, global_idx * 16 + 4, global_id_y));
            EXPECT_TRUE(CheckEqual<uint32_t>(*global_id, global_idx * 16 + 8, global_id_z));

            EXPECT_TRUE(CheckEqual<uint32_t>(*group_id, global_idx * 16 + 0, gx));
            EXPECT_TRUE(CheckEqual<uint32_t>(*group_id, global_idx * 16 + 4, gy));
            EXPECT_TRUE(CheckEqual<uint32_t>(*group_id, global_idx * 16 + 8, gz));

            EXPECT_TRUE(CheckEqual<uint32_t>(*num_groups, global_idx * 16 + 0, num_groups_x));
            EXPECT_TRUE(CheckEqual<uint32_t>(*num_groups, global_idx * 16 + 4, num_groups_y));
            EXPECT_TRUE(CheckEqual<uint32_t>(*num_groups, global_idx * 16 + 8, num_groups_z));
        };
        for (uint32_t gz = 0; gz < num_groups_z; gz++) {
            for (uint32_t gy = 0; gy < num_groups_y; gy++) {
                for (uint32_t gx = 0; gx < num_groups_x; gx++) {
                    for (uint32_t lz = 0; lz < wgsize_z; lz++) {
                        for (uint32_t ly = 0; ly < wgsize_y; ly++) {
                            for (uint32_t lx = 0; lx < wgsize_x; lx++) {
                                check(lx, ly, lz, gx, gy, gz);
                            }
                        }
                    }
                }
            }
        }
    }
};

TEST_F(ComputeBuiltinsTest, Param) {
    Run(R"(
@group(0) @binding(0) var<storage, read_write> local_id_out : array<vec3<u32>>;
@group(0) @binding(1) var<storage, read_write> local_index_out : array<u32>;
@group(0) @binding(2) var<storage, read_write> global_id_out : array<vec3<u32>>;
@group(0) @binding(3) var<storage, read_write> group_id_out : array<vec3<u32>>;
@group(0) @binding(4) var<storage, read_write> num_groups_out : array<vec3<u32>>;

const wgsize_x = 4u;
const wgsize_y = 3u;
const wgsize_z = 2u;
@compute @workgroup_size(wgsize_x, wgsize_y, wgsize_z)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) local_index : u32,
        @builtin(global_invocation_id) global_id : vec3<u32>,
        @builtin(workgroup_id) group_id : vec3<u32>,
        @builtin(num_workgroups) num_groups : vec3<u32>,
) {
  let wgsize = wgsize_x * wgsize_y * wgsize_z;
  let group_idx = ((group_id.z * num_groups.y) + group_id.y) * num_groups.x + group_id.x;
  let global_idx = group_idx * wgsize + local_index;
  local_id_out[global_idx] = local_id;
  local_index_out[global_idx] = local_index;
  global_id_out[global_idx] = global_id;
  group_id_out[global_idx] = group_id;
  num_groups_out[global_idx] = num_groups;
}
)");
}

TEST_F(ComputeBuiltinsTest, Struct) {
    Run(R"(
@group(0) @binding(0) var<storage, read_write> local_id_out : array<vec3<u32>>;
@group(0) @binding(1) var<storage, read_write> local_index_out : array<u32>;
@group(0) @binding(2) var<storage, read_write> global_id_out : array<vec3<u32>>;
@group(0) @binding(3) var<storage, read_write> group_id_out : array<vec3<u32>>;
@group(0) @binding(4) var<storage, read_write> num_groups_out : array<vec3<u32>>;

struct Local {
  @builtin(local_invocation_id) id : vec3<u32>,
  @builtin(local_invocation_index) index : u32,
}

struct Global {
  @builtin(global_invocation_id) id : vec3<u32>,
}

struct Group {
  @builtin(workgroup_id) id : vec3<u32>,
  @builtin(num_workgroups) num : vec3<u32>,
}

const wgsize_x = 4u;
const wgsize_y = 3u;
const wgsize_z = 2u;
@compute @workgroup_size(wgsize_x, wgsize_y, wgsize_z)
fn main(local : Local, global : Global, group : Group) {
  let wgsize = wgsize_x * wgsize_y * wgsize_z;
  let group_idx = ((group.id.z * group.num.y) + group.id.y) * group.num.x + group.id.x;
  let global_idx = group_idx * wgsize + local.index;
  local_id_out[global_idx] = local.id;
  local_index_out[global_idx] = local.index;
  global_id_out[global_idx] = global.id;
  group_id_out[global_idx] = group.id;
  num_groups_out[global_idx] = group.num;
}
)");
}

TEST_F(ComputeEndToEndTest, PipelineOverrides) {
    NamedOverrideList overrides;
    overrides["a"] = -10;
    overrides["b"] = 0.25;
    overrides["c"] = true;
    overrides["53"] = 99;
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a_result : i32;
@group(0) @binding(1) var<storage, read_write> b_result : f32;
@group(0) @binding(2) var<storage, read_write> c_result : u32;
@group(0) @binding(3) var<storage, read_write> d_result : u32;
@group(0) @binding(4) var<storage, read_write> e_result : u32;

override a : i32 = 100;    // Overriden to -10
override b : f32;          // Overriden to 0.25
override c : bool = false; // Overriden to `true`
override d : u32 = 42;     // Not overriden
@id(53) override e : u32;  // Overriden by id
override f : u32;          // Not used, no initializer

@compute @workgroup_size(1)
fn main() {
  a_result = a;
  b_result = 3 + new_b;

  if (c) {
    c_result = 7;
  } else {
    c_result = 100;
  }

  d_result = d;
  e_result = e;
}

override new_b = b * 2;

@compute @workgroup_size(f)
fn foo() {
  _ = f;
}

)",
         std::move(overrides));

    auto a_result = MakeBuffer<int32_t>({});
    auto b_result = MakeBuffer<float>({});
    auto c_result = MakeBuffer<uint32_t>({});
    auto d_result = MakeBuffer<uint32_t>({});
    auto e_result = MakeBuffer<uint32_t>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a_result.get(), 0, a_result->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(b_result.get(), 0, b_result->Size());
    bindings[{0, 2}] = Binding::MakeBufferBinding(c_result.get(), 0, c_result->Size());
    bindings[{0, 3}] = Binding::MakeBufferBinding(d_result.get(), 0, d_result->Size());
    bindings[{0, 4}] = Binding::MakeBufferBinding(e_result.get(), 0, e_result->Size());
    ASSERT_EQ(executor_->Run(UVec3(1, 1, 1), std::move(bindings)), Success);

    EXPECT_TRUE(CheckEqual<int32_t>(*a_result, 0, -10));
    EXPECT_TRUE(CheckEqual<float>(*b_result, 0, 3.5));
    EXPECT_TRUE(CheckEqual<uint32_t>(*c_result, 0, 7));
    EXPECT_TRUE(CheckEqual<uint32_t>(*d_result, 0, 42));
    EXPECT_TRUE(CheckEqual<uint32_t>(*e_result, 0, 99));
}

class ComputeWorkgroupSizeTest : public ComputeEndToEndTest {
  protected:
    void Run(uint32_t wgx,
             uint32_t wgy,
             uint32_t wgz,
             NamedOverrideList&& overrides,
             const char* wgsl) {
        Init(wgsl, std::move(overrides));

        uint32_t wgsize = wgx * wgy * wgz;
        auto buffer = MakeZeroInitBuffer(wgsize * 16);

        BindingList bindings;
        bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
        ASSERT_EQ(executor_->Run(UVec3(1, 1, 1), std::move(bindings)), Success);
        for (size_t z = 0; z < wgz; z++) {
            for (size_t y = 0; y < wgy; y++) {
                for (size_t x = 0; x < wgx; x++) {
                    size_t idx = x + (y + (z * wgy)) * wgx;
                    EXPECT_TRUE(
                        CheckEqual<uint32_t>(*buffer, idx * 16 + 0, static_cast<uint32_t>(x)));
                    EXPECT_TRUE(
                        CheckEqual<uint32_t>(*buffer, idx * 16 + 4, static_cast<uint32_t>(y)));
                    EXPECT_TRUE(
                        CheckEqual<uint32_t>(*buffer, idx * 16 + 8, static_cast<uint32_t>(z)));
                }
            }
        }
    }
};

TEST_F(ComputeWorkgroupSizeTest, Literals_AllSpecified) {
    uint32_t wgx = 3;
    uint32_t wgy = 2;
    uint32_t wgz = 5;
    Run(wgx, wgy, wgz, {}, R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

@compute @workgroup_size(3, 2, 5)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}
)");
}

TEST_F(ComputeWorkgroupSizeTest, Literals_NotAllSpecified) {
    uint32_t wgx = 10;
    uint32_t wgy = 1;
    uint32_t wgz = 1;
    Run(wgx, wgy, wgz, {}, R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

@compute @workgroup_size(10)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}
)");
}

TEST_F(ComputeWorkgroupSizeTest, Constants) {
    uint32_t wgx = 5;
    uint32_t wgy = 4;
    uint32_t wgz = 3;
    Run(wgx, wgy, wgz, {}, R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

const wgx = 5;
const wgy = 4;
const wgz = 3;

@compute @workgroup_size(wgx, wgy, wgz)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}
)");
}

TEST_F(ComputeWorkgroupSizeTest, Defaults_NotOverriden) {
    uint32_t wgx = 3;
    uint32_t wgy = 2;
    uint32_t wgz = 1;
    Run(wgx, wgy, wgz, {}, R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

override wgx : i32 = 3;
override wgy : i32 = 2;
override wgz : i32 = 1;

@compute @workgroup_size(wgx, wgy, wgz)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}
)");
}

TEST_F(ComputeWorkgroupSizeTest, Defaults_Overriden) {
    uint32_t wgx = 3;
    uint32_t wgy = 2;
    uint32_t wgz = 1;

    NamedOverrideList overrides;
    overrides["wgx"] = wgx;
    overrides["wgy"] = wgy;
    overrides["wgz"] = wgz;

    Run(wgx, wgy, wgz, std::move(overrides), R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

override wgx : i32 = 10;
override wgy : i32 = 10;
override wgz : i32 = 10;

@compute @workgroup_size(wgx, wgy, wgz)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}
)");
}

TEST_F(ComputeWorkgroupSizeTest, NoInitializer_Overridden) {
    uint32_t wgx = 4;
    uint32_t wgy = 2;
    uint32_t wgz = 3;

    NamedOverrideList overrides;
    overrides["wgx"] = wgx;
    overrides["wgy"] = wgy;
    overrides["wgz"] = wgz;

    Run(wgx, wgy, wgz, std::move(overrides), R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

override wgx : i32;
override wgy : i32;
override wgz : i32;

@compute @workgroup_size(wgx, wgy, wgz)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}
)");
}

TEST_F(ComputeWorkgroupSizeTest, ComplexExpressions) {
    uint32_t wgx = 2;
    uint32_t wgy = 3;
    uint32_t wgz = 1;

    NamedOverrideList overrides;
    overrides["wgx"] = wgx;
    overrides["wgy"] = wgy;
    overrides["wgz"] = wgz;

    Run(wgx * 2, wgy + wgx, (2 + wgz) * wgx - 1, std::move(overrides), R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec3<u32>>;

override wgx : i32 = 10;
override wgy : i32 = 20;
override wgz : i32 = 30;

override new_wgy = wgy + wgx;

@compute @workgroup_size(wgx * 2, new_wgy, new_wgz)
fn main(@builtin(local_invocation_id) local_id : vec3<u32>,
        @builtin(local_invocation_index) idx : u32) {
  buffer[idx] = local_id;
}

// Make sure declaration order doesn't matter.
override new_wgz = (temp_wgz * wgx) - 1;
override temp_wgz = 2 + wgz;

)");
}

TEST_F(ComputeEndToEndTest, TypeConversions) {
    Init(R"(
struct S {
  i1 : i32,
  i2 : i32,
  i3 : i32,
  i4 : i32,
  u1 : u32,
  u2 : u32,
  u3 : u32,
  u4 : u32,
  f1 : f32,
  f2 : f32,
  f3 : f32,
  f4 : f32,

  v2i : vec2<i32>,
  v3u : vec3<u32>,
  v4f : vec4<f32>,
}

@group(0) @binding(0) var<storage, read_write> output : S;

@compute @workgroup_size(1)
fn main() {
  var i = 7i;
  var u = 42u;
  var f = 10.5f;
  var b = true;
  var v2f = vec2<f32>(10.75, 7.25);
  var v3i = vec3<i32>(10, 0, 7);
  var v4u = vec4<u32>(0, 1, 100, 4000000000);

  output.i1 = i32(i);
  output.i2 = i32(u);
  output.i3 = i32(f);
  output.i4 = i32(b);

  output.u1 = u32(i);
  output.u2 = u32(u);
  output.u3 = u32(f);
  output.u4 = u32(b);

  output.f1 = f32(i);
  output.f2 = f32(u);
  output.f3 = f32(f);
  output.f4 = f32(b);

  output.v2i = vec2<i32>(v2f);
  output.v3u = vec3<u32>(v3i);
  output.v4f = vec4<f32>(v4u);
}
)");

    auto buffer = MakeBuffer<int32_t, 32>({0});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));

    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 0, static_cast<int32_t>(7)));
    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 4, static_cast<int32_t>(42)));
    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 8, static_cast<int32_t>(10.5)));
    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 12, static_cast<int32_t>(true)));

    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 16, static_cast<uint32_t>(7)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 20, static_cast<uint32_t>(42)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 24, static_cast<uint32_t>(10.5)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 28, static_cast<uint32_t>(true)));

    EXPECT_TRUE(CheckEqual<float>(*buffer, 32, static_cast<float>(7)));
    EXPECT_TRUE(CheckEqual<float>(*buffer, 36, static_cast<float>(42)));
    EXPECT_TRUE(CheckEqual<float>(*buffer, 40, static_cast<float>(10.5)));
    EXPECT_TRUE(CheckEqual<float>(*buffer, 44, static_cast<float>(true)));

    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 48, static_cast<int32_t>(10)));
    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, 52, static_cast<int32_t>(7)));

    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 64, static_cast<uint32_t>(10)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 68, static_cast<uint32_t>(0)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, 72, static_cast<uint32_t>(7)));

    EXPECT_TRUE(CheckEqual<float>(*buffer, 80, static_cast<float>(0)));
    EXPECT_TRUE(CheckEqual<float>(*buffer, 84, static_cast<float>(1)));
    EXPECT_TRUE(CheckEqual<float>(*buffer, 88, static_cast<float>(100)));
    EXPECT_TRUE(CheckEqual<float>(*buffer, 92, static_cast<float>(4000000000)));
}

class BinaryOpTestBase : protected ComputeEndToEndTestBase,
                         public testing::TestWithParam<std::string> {
  protected:
    template <typename T, unsigned VecWidth, typename F>
    void Run(std::string wgsl_type, F&& ref) {
        auto& op = GetParam();

        Init(R"(
alias T = )" +
             wgsl_type +
             R"(;
@group(0) @binding(0) var<storage, read> a : array<T>;
@group(0) @binding(1) var<storage, read> b : array<T>;
@group(0) @binding(2) var<storage, read_write> output : array<T>;

@compute @workgroup_size(1)
fn main() {
  for (var i = 0; i < 4/)" +
             std::to_string(VecWidth) + R"(; i++) {
    output[i] = T(a[i] )" +
             op + R"( b[i]);
  }
})");

        // Set up the LHS and RHS values.
        // We don't need to be too thorough here as the const-eval tests cover this comprehensively.
        std::array<T, 4> a;
        std::array<T, 4> b;
        if constexpr (std::is_floating_point<T>()) {
            a = {4.5f, 100.0f, -0.05f, 0.f};
            b = {3.25f, -0.0025f, -22.125f, 1.f};
        } else if constexpr (std::is_signed<T>()) {
            a = {4, 100, -50, 0};
            b = {3, -17, -22, 1};
        } else if constexpr (std::is_unsigned<T>()) {
            a = {4, 100, 50, 0};
            b = {3, 17, 22, 1};
        }

        auto a_buffer = MakeBuffer<T, 4>(a);
        auto b_buffer = MakeBuffer<T, 4>(b);
        auto output = MakeBuffer<T, 4>({0});

        BindingList bindings;
        bindings[{0, 0}] = Binding::MakeBufferBinding(a_buffer.get(), 0, a_buffer->Size());
        bindings[{0, 1}] = Binding::MakeBufferBinding(b_buffer.get(), 0, b_buffer->Size());
        bindings[{0, 2}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
        EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
        for (size_t i = 0; i < 4; i++) {
            if (VecWidth == 3 && i == 3) {
                break;
            }
            EXPECT_TRUE(CheckEqual<T>(*output, i * sizeof(T), ref(a[i], b[i])));
        }
    }
};

class ArithmeticBinaryOpTest : public BinaryOpTestBase {
  protected:
    template <typename T, unsigned VecWidth = 1>
    void Run(std::string wgsl_type) {
        auto& op = GetParam();
        BinaryOpTestBase::Run<T, VecWidth>(wgsl_type, [&](T a, T b) -> T {
            if (op == "+") {
                return a + b;
            } else if (op == "-") {
                return a - b;
            } else if (op == "*") {
                return a * b;
            } else if (op == "/") {
                return a / b;
            } else if (op == "%") {
                if constexpr (std::is_integral<T>()) {
                    return a % b;
                } else {
                    return a - b * std::trunc(a / b);
                }
            } else {
                return 0;
            }
        });
    }
};

TEST_P(ArithmeticBinaryOpTest, Scalar_I32) {
    Run<int32_t>("i32");
}

TEST_P(ArithmeticBinaryOpTest, Scalar_U32) {
    Run<uint32_t>("u32");
}

TEST_P(ArithmeticBinaryOpTest, Scalar_F32) {
    Run<float>("f32");
}

TEST_P(ArithmeticBinaryOpTest, Vec2_I32) {
    Run<int32_t, 2>("vec2<i32>");
}

TEST_P(ArithmeticBinaryOpTest, Vec3_U32) {
    Run<uint32_t, 3>("vec3<u32>");
}

TEST_P(ArithmeticBinaryOpTest, Vec4_F32) {
    Run<float, 4>("vec4<f32>");
}

TEST_P(ArithmeticBinaryOpTest, Mat2x2_F32) {
    if (GetParam() == "+" || GetParam() == "-") {
        Run<float, 4>("mat2x2<f32>");
    }
}

INSTANTIATE_TEST_SUITE_P(ComputeEndToEndTest,
                         ArithmeticBinaryOpTest,
                         ::testing::Values("+", "-", "*", "/", "%"));

class BitwiseBinaryOpTest : public BinaryOpTestBase {
  protected:
    template <typename T, unsigned VecWidth = 1>
    void Run(std::string wgsl_type) {
        auto& op = GetParam();
        BinaryOpTestBase::Run<T, VecWidth>(wgsl_type, [&](T a, T b) -> T {
            if (op == "&") {
                return a & b;
            } else if (op == "|") {
                return a | b;
            } else if (op == "^") {
                return a ^ b;
            } else {
                return 0;
            }
        });
    }
};

TEST_P(BitwiseBinaryOpTest, Scalar_I32) {
    Run<int32_t>("i32");
}

TEST_P(BitwiseBinaryOpTest, Scalar_U32) {
    Run<uint32_t>("u32");
}

TEST_P(BitwiseBinaryOpTest, Vec2_I32) {
    Run<int32_t, 2>("vec2<i32>");
}

TEST_P(BitwiseBinaryOpTest, Vec3_U32) {
    Run<uint32_t, 3>("vec3<u32>");
}

TEST_P(BitwiseBinaryOpTest, Vec4_U32) {
    Run<uint32_t, 4>("vec4<u32>");
}

INSTANTIATE_TEST_SUITE_P(ComputeEndToEndTest,
                         BitwiseBinaryOpTest,
                         ::testing::Values("&", "|", "^"));

class ComparisonBinaryOpTest : public BinaryOpTestBase {
  protected:
    template <typename T, unsigned VecWidth = 1>
    void Run(std::string wgsl_type) {
        auto& op = GetParam();
        BinaryOpTestBase::Run<T, VecWidth>(wgsl_type, [&](T a, T b) -> T {
            if (op == "==") {
                if constexpr (std::is_floating_point_v<T>) {
                    T diff = std::abs(a - b);
                    return diff <= 0.000001f;
                } else {
                    return a == b;
                }
            } else if (op == "!=") {
                if constexpr (std::is_floating_point_v<T>) {
                    T diff = std::abs(a - b);
                    return diff > 0.000001f;
                } else {
                    return a != b;
                }
            } else if (op == "<") {
                return a < b;
            } else if (op == "<=") {
                return a <= b;
            } else if (op == ">") {
                return a > b;
            } else if (op == ">=") {
                return a >= b;
            } else {
                return 0;
            }
        });
    }
};

TEST_P(ComparisonBinaryOpTest, Scalar_I32) {
    Run<int32_t>("i32");
}

TEST_P(ComparisonBinaryOpTest, Scalar_U32) {
    Run<uint32_t>("u32");
}

TEST_P(ComparisonBinaryOpTest, Scalar_F32) {
    Run<float>("f32");
}

TEST_P(ComparisonBinaryOpTest, Vec2_I32) {
    Run<int32_t, 2>("vec2<i32>");
}

TEST_P(ComparisonBinaryOpTest, Vec3_U32) {
    Run<uint32_t, 3>("vec3<u32>");
}

TEST_P(ComparisonBinaryOpTest, Vec4_F32) {
    Run<float, 4>("vec4<f32>");
}

INSTANTIATE_TEST_SUITE_P(ComputeEndToEndTest,
                         ComparisonBinaryOpTest,
                         ::testing::Values("==", "!=", "<", "<=", ">", ">="));

TEST_F(ComputeEndToEndTest, BoolComparison) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> output : array<u32, 12>;

@compute @workgroup_size(1)
fn main() {
  var t = true;
  var f = false;
  output[0] = u32(t == t);
  output[1] = u32(t == f);
  output[2] = u32(f == f);
  output[3] = u32(t != t);
  output[4] = u32(t != f);
  output[5] = u32(f != f);
  output[6] = u32(t & t);
  output[7] = u32(t & f);
  output[8] = u32(f & f);
  output[9] = u32(t | t);
  output[10] = u32(t | f);
  output[11] = u32(f | f);
}
)");

    auto buffer = MakeBuffer<uint32_t, 12>({0});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, {1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1, 0}));
}

TEST_F(ComputeEndToEndTest, Unary_Minus) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<i32, 6>;

@compute @workgroup_size(1)
fn main() {
  buffer[0] = -buffer[0];
  buffer[1] = -buffer[1];
  var v = vec4(buffer[2], buffer[3], buffer[4], buffer[5]);
  var nv = -v;
  buffer[2] = nv.x;
  buffer[3] = nv.y;
  buffer[4] = nv.z;
  buffer[5] = nv.w;
}
)");

    auto buffer = MakeBuffer<int32_t, 6>({0, 1, -1, 123456789, 2147483647, -2147483647});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*buffer, {0, -1, 1, -123456789, -2147483647, 2147483647}));
}

TEST_F(ComputeEndToEndTest, Unary_LogicalNot) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> output : array<u32, 6>;

@compute @workgroup_size(1)
fn main() {
  var t = true;
  var f = false;
  var v = vec4<bool>(t, f, f, t);
  output[0] = u32(!t);
  output[1] = u32(!f);
  var nv = !v;
  output[2] = u32(nv.x);
  output[3] = u32(nv.y);
  output[4] = u32(nv.z);
  output[5] = u32(nv.w);
}
)");

    auto buffer = MakeBuffer<uint32_t, 6>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*buffer, {0, 1, 0, 1, 1, 0}));
}

TEST_F(ComputeEndToEndTest, Unary_BitwiseComplement) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<u32, 6>;

@compute @workgroup_size(1)
fn main() {
  buffer[0] = ~buffer[0];
  buffer[1] = ~buffer[1];
  var v = vec4(buffer[2], buffer[3], buffer[4], buffer[5]);
  var cv = ~v;
  buffer[2] = cv.x;
  buffer[3] = cv.y;
  buffer[4] = cv.z;
  buffer[5] = cv.w;
}
)");

    auto buffer = MakeBuffer<uint32_t, 6>(
        {0x00000000, 0xFFFFFFFF, 0x0F0F0F0F, 0xF0F0F0F0, 0x88888888, 0x77777777});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(
        *buffer, {0xFFFFFFFF, 0x00000000, 0xF0F0F0F0, 0x0F0F0F0F, 0x77777777, 0x88888888}));
}

TEST_F(ComputeEndToEndTest, AtomicLoadStore) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicLoad(&a);
  atomicStore(&a, id.x);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<int32_t>({0});
    auto out = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*a, 0, 3));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {0, 0, 1, 2}));
}

TEST_F(ComputeEndToEndTest, AtomicAdd) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<i32>;
@group(0) @binding(1) var<storage, read_write> out : array<i32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicAdd(&a, 1);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<int32_t>({0});
    auto out = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*a, 0, 4));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {0, 1, 2, 3}));
}

TEST_F(ComputeEndToEndTest, AtomicSub) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicSub(&a, 1);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<uint32_t>({4000000000});
    auto out = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*a, 0, 3999999996));
    EXPECT_TRUE(CheckEqual<uint32_t>(*out, {4000000000, 3999999999, 3999999998, 3999999997}));
}

TEST_F(ComputeEndToEndTest, AtomicMax) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<i32>;
@group(0) @binding(1) var<storage, read_write> out : array<i32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicMax(&a, i32(id.x) - 2);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<int32_t>({-10});
    auto out = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*a, 0, 1));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {-10, -2, -1, 0}));
}

TEST_F(ComputeEndToEndTest, AtomicMin) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<i32>;
@group(0) @binding(1) var<storage, read_write> out : array<i32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicMin(&a, 2 - i32(id.x));
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<int32_t>({10});
    auto out = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*a, 0, -1));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {10, 2, 1, 0}));
}

TEST_F(ComputeEndToEndTest, AtomicAnd) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicAnd(&a, 0xFFu >> (id.x + 1));
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<uint32_t>({0xFF});
    auto out = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*a, 0, 0x0F));
    EXPECT_TRUE(CheckEqual<uint32_t>(*out, {0xFF, 0x7F, 0x3F, 0x1F}));
}

TEST_F(ComputeEndToEndTest, AtomicOr) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicOr(&a, 1u << id.x);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<uint32_t>({});
    auto out = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*a, 0, 0x0F));
    EXPECT_TRUE(CheckEqual<uint32_t>(*out, {0x00, 0x01, 0x03, 0x07}));
}

TEST_F(ComputeEndToEndTest, AtomicXor) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicXor(&a, 3u << id.x);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<uint32_t>({});
    auto out = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*a, 0, 0x11));
    EXPECT_TRUE(CheckEqual<uint32_t>(*out, {0x00, 0x03, 0x05, 0x09}));
}

TEST_F(ComputeEndToEndTest, AtomicExchange) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let old = atomicExchange(&a, id.x * 2);
  out[id.x] = old;
}
)");

    auto a = MakeBuffer<uint32_t>({});
    auto out = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*a, 0, 6));
    EXPECT_TRUE(CheckEqual<uint32_t>(*out, {0, 0, 2, 4}));
}

TEST_F(ComputeEndToEndTest, AtomicCompareExchange) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> a : atomic<u32>;
@group(0) @binding(1) var<storage, read_write> out : array<u32, 4>;

@compute @workgroup_size(1)
fn main(@builtin(global_invocation_id) id : vec3<u32>) {
  let xchg = atomicCompareExchangeWeak(&a, id.x / 2, id.x + 1);
  out[id.x] = xchg.old_value;
}
)");

    auto a = MakeBuffer<uint32_t>({});
    auto out = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(a.get(), 0, a->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(4, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*a, 0, 3));
    EXPECT_TRUE(CheckEqual<uint32_t>(*out, {0, 1, 1, 3}));
}

TEST_F(ComputeEndToEndTest, StorageBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> out : array<u32, 5>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 3) {
    out[idx] = 10;
  }
  for (var i = 0; i < 4; i++) {
    storageBarrier();
    let next = out[idx + 1];
    storageBarrier();
    if (next > 0) {
      out[idx] = next + 1;
    }
  }
}
)");

    auto out = MakeBuffer<int32_t, 5>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {13, 12, 11, 10}));
}

TEST_F(ComputeEndToEndTest, WorkgroupBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> out : array<u32, 4>;

var<workgroup> scratch : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 3) {
    scratch = 10;
  }
  workgroupBarrier();
  out[idx] = scratch + idx;
}
)");

    auto out = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {10, 11, 12, 13}));
}

TEST_F(ComputeEndToEndTest, WorkgroupUniformLoad) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> out : array<u32, 4>;

var<workgroup> scratch : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  // Make sure it works if the result is assigned to a phony.
  _ = workgroupUniformLoad(&scratch);

  if (idx == 3) {
    scratch = 10;
  }
  let value = workgroupUniformLoad(&scratch);
  out[idx] = value + idx;
}
)");

    auto out = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {10, 11, 12, 13}));
}

TEST_F(ComputeEndToEndTest, NamedPipelineOverridableWorkgroupArraySize) {
    Init(R"(
override wgx : i32;
override wgy : i32;

@group(0) @binding(0) var<storage, read_write> out : array<i32, 8>;

override size = wgx * wgy;
var<workgroup> scratch : array<i32, size>;

@compute @workgroup_size(wgx, wgy)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    for (var i = 0; i < size; i++) {
      scratch[i] = i;
    }
  }
  workgroupBarrier();
  out[idx] = scratch[idx];
}
)",
         {{"wgx", 4}, {"wgy", 2}});

    auto out = MakeBuffer<int32_t, 8>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST_F(ComputeEndToEndTest, UnnamedPipelineOverridableWorkgroupArraySize) {
    Init(R"(
override wgx : i32;

@group(0) @binding(0) var<storage, read_write> out : array<i32, 8>;

var<workgroup> scratch : array<i32, 2 * wgx>;

@compute @workgroup_size(wgx, 2)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    for (var i = 0; i < wgx * 2; i++) {
      scratch[i] = i;
    }
  }
  workgroupBarrier();
  out[idx] = scratch[idx];
}
)",
         {{"wgx", 4}});

    auto out = MakeBuffer<int32_t, 8>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(out.get(), 0, out->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*out, {0, 1, 2, 3, 4, 5, 6, 7}));
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_ValueArray) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

const values = array(1, 2, 3);

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  output[idx] = values[idx];
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:8:17 warning: index 3 is out of bounds
  output[idx] = values[idx];
                ^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_UniformBuffer) {
    Init(R"(
@group(0) @binding(0) var<uniform> input : array<vec4<i32>, 3>;
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  output[idx] = input[idx].x;
}
)");

    auto input = MakeBuffer<int32_t, 12>({1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0});
    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(input.get(), 0, input->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:7:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[idx].x;
                ^^^^^^^^^^^^

test.wgsl:2:36 note: accessing 48 byte allocation in the uniform address space
@group(0) @binding(0) var<uniform> input : array<vec4<i32>, 3>;
                                   ^^^^^

test.wgsl:7:17 note: created a 16 byte memory view at an offset of 48 bytes
  output[idx] = input[idx].x;
                ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_StorageBuffer) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> input : array<i32>;
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  output[idx] = input[idx];
}
)");

    auto input = MakeBuffer<int32_t, 3>({1, 2, 3});
    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(input.get(), 0, input->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:7:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[idx];
                ^^^^^^^^^^

test.wgsl:2:48 note: accessing 12 byte allocation in the storage address space
@group(0) @binding(0) var<storage, read_write> input : array<i32>;
                                               ^^^^^

test.wgsl:7:17 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = input[idx];
                ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsWrite_StorageBuffer) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> input : array<i32>;
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  output[idx] = input[idx];
}
)");

    auto input = MakeBuffer<int32_t, 4>({1, 2, 3, 4});
    auto output = MakeBuffer<int32_t, 3>({});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(input.get(), 0, input->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:7:15 warning: storing to an out-of-bounds memory view
  output[idx] = input[idx];
              ^

test.wgsl:3:48 note: accessing 12 byte allocation in the storage address space
@group(0) @binding(1) var<storage, read_write> output : array<i32>;
                                               ^^^^^^

test.wgsl:7:3 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = input[idx];
  ^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_Private) {
    Init(R"(
var<private> input : array<i32, 3>;
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  input = array(1, 2, 3);
  output[idx] = input[idx];
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:8:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[idx];
                ^^^^^^^^^^

test.wgsl:2:14 note: accessing 12 byte allocation in the private address space
var<private> input : array<i32, 3>;
             ^^^^^

test.wgsl:8:17 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = input[idx];
                ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsWrite_Private) {
    Init(R"(
var<private> input : array<u32, 3>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  input[idx] = idx;
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(test.wgsl:6:14 warning: storing to an out-of-bounds memory view
  input[idx] = idx;
             ^

test.wgsl:2:14 note: accessing 12 byte allocation in the private address space
var<private> input : array<u32, 3>;
             ^^^^^

test.wgsl:6:3 note: created a 4 byte memory view at an offset of 12 bytes
  input[idx] = idx;
  ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_Function) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input : array<i32, 3>;
  input = array(1, 2, 3);
  output[idx] = input[idx];
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:8:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[idx];
                ^^^^^^^^^^

test.wgsl:6:3 note: accessing 12 byte allocation in the function address space
  var input : array<i32, 3>;
  ^^^^^^^^^^^^^^^^^^^^^^^^^

test.wgsl:8:17 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = input[idx];
                ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsWrite_Function) {
    Init(R"(
@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input : array<u32, 3>;
  input[idx] = idx;
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(test.wgsl:5:14 warning: storing to an out-of-bounds memory view
  input[idx] = idx;
             ^

test.wgsl:4:3 note: accessing 12 byte allocation in the function address space
  var input : array<u32, 3>;
  ^^^^^^^^^^^^^^^^^^^^^^^^^

test.wgsl:5:3 note: created a 4 byte memory view at an offset of 12 bytes
  input[idx] = idx;
  ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_Workgroup) {
    Init(R"(
var<workgroup> input : array<i32, 3>;
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    input = array(1, 2, 3);
  }
  workgroupBarrier();
  output[idx] = input[idx];
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:11:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[idx];
                ^^^^^^^^^^

test.wgsl:2:16 note: accessing 12 byte allocation in the workgroup address space
var<workgroup> input : array<i32, 3>;
               ^^^^^

test.wgsl:11:17 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = input[idx];
                ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsWrite_Workgroup) {
    Init(R"(
var<workgroup> input : array<u32, 3>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  input[idx] = idx;
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(test.wgsl:6:14 warning: storing to an out-of-bounds memory view
  input[idx] = idx;
             ^

test.wgsl:2:16 note: accessing 12 byte allocation in the workgroup address space
var<workgroup> input : array<u32, 3>;
               ^^^^^

test.wgsl:6:3 note: created a 4 byte memory view at an offset of 12 bytes
  input[idx] = idx;
  ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsWrite_ViaPointerParameter) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

fn foo(p : ptr<function, i32>) -> i32 {
  return 1 + *p;
}

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input = array(1, 2, 3);
  output[idx] = foo(&input[idx]);
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {2, 3, 4, 1}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:5:14 warning: loading from an out-of-bounds memory view
  return 1 + *p;
             ^^

test.wgsl:10:3 note: accessing 12 byte allocation in the function address space
  var input = array(1, 2, 3);
  ^^^^^^^^^

test.wgsl:11:22 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = foo(&input[idx]);
                     ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_IndexChain_BaseInvalid) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(3)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input = array(array(array(1, 2, 3), array(4, 5, 6)), array(array(9, 8, 7), array(6, 5, 4)));
  output[idx] = input[idx][0][0];
}
)");

    auto output = MakeBuffer<int32_t, 3>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 9, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:7:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[idx][0][0];
                ^^^^^^^^^^^^^^^^

test.wgsl:6:3 note: accessing 48 byte allocation in the function address space
  var input = array(array(array(1, 2, 3), array(4, 5, 6)), array(array(9, 8, 7), array(6, 5, 4)));
  ^^^^^^^^^

test.wgsl:7:17 note: created a 24 byte memory view at an offset of 48 bytes
  output[idx] = input[idx][0][0];
                ^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsRead_IndexChain_LeafInvalid) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input = array(array(array(1, 2, 3), array(4, 5, 6)), array(array(9, 8, 7), array(6, 5, 4)));
  output[idx] = input[0][0][idx];
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 2, 3, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:7:17 warning: loading from an out-of-bounds memory view
  output[idx] = input[0][0][idx];
                ^^^^^^^^^^^^^^^^

test.wgsl:6:3 note: accessing 48 byte allocation in the function address space
  var input = array(array(array(1, 2, 3), array(4, 5, 6)), array(array(9, 8, 7), array(6, 5, 4)));
  ^^^^^^^^^

test.wgsl:7:17 note: created a 4 byte memory view at an offset of 12 bytes
  output[idx] = input[0][0][idx];
                ^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBounds_Atomic) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<atomic<i32>>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  atomicAdd(&output[idx], 1);
}
)");

    auto output = MakeBuffer<int32_t, 3>({1, 2, 3});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {2, 3, 4}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:3 warning: atomic operation on an out-of-bounds memory view
  atomicAdd(&output[idx], 1);
  ^^^^^^^^^

test.wgsl:2:48 note: accessing 12 byte allocation in the storage address space
@group(0) @binding(1) var<storage, read_write> output : array<atomic<i32>>;
                                               ^^^^^^

test.wgsl:6:14 note: created a 4 byte memory view at an offset of 12 bytes
  atomicAdd(&output[idx], 1);
             ^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, OutOfBoundsMemoryView_NeverUsed) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<u32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input = array(1, 2, 3);
  let p = &input[idx];
  output[idx] = idx;
}
)");

    auto output = MakeBuffer<uint32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<uint32_t>(*output, {0, 1, 2, 3}));
}

TEST_F(ComputeEndToEndTest, OutOfBoundsMemoryView_NeverUsed_ViaPointerParameter) {
    Init(R"(
@group(0) @binding(1) var<storage, read_write> output : array<i32>;

fn foo(p : ptr<function, i32>) -> i32 {
  return 1;
}

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var input = array(1, 2, 3);
  output[idx] = foo(&input[idx]);
}
)");

    auto output = MakeBuffer<int32_t, 4>({});

    BindingList bindings;
    bindings[{0, 1}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings)));
    EXPECT_TRUE(CheckEqual<int32_t>(*output, {1, 1, 1, 1}));
}

TEST_F(ComputeEndToEndTest, ConstEvalError_LoadNonFinite_F32) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var f : f32 = buffer[idx];
}
)");

    auto output = MakeBuffer<uint32_t, 4>({0x00000000, 0x3F800000, 0x7F800000, 0x7FFFFFFF});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:17 warning: loading a non-finite f32 value (inf)
  var f : f32 = buffer[idx];
                ^^^^^^^^^^^

test.wgsl:6:17 warning: loading a non-finite f32 value (nan)
  var f : f32 = buffer[idx];
                ^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_LoadNonFinite_F16) {
    Init(R"(
enable f16;

@group(0) @binding(0) var<storage, read_write> buffer : array<f16>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  var f : f16 = buffer[idx];
}
)");

    auto output = MakeBuffer<uint16_t, 4>({0x0000, 0x3F80, 0x7C00, 0x7FFF});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));

    EXPECT_EQ(errors_,
              R"(test.wgsl:8:17 warning: loading a non-finite f16 value (inf)
  var f : f16 = buffer[idx];
                ^^^^^^^^^^^

test.wgsl:8:17 warning: loading a non-finite f16 value (nan)
  var f : f16 = buffer[idx];
                ^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_BitcastNaN) {
    Init(R"(
@compute @workgroup_size(1)
fn main(@builtin(local_invocation_index) idx : u32) {
  var u : u32 = 0x7FFFFFFFu;
  var f : f32 = bitcast<f32>(u);
}
)");

    BindingList bindings;
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));

    EXPECT_EQ(errors_,
              R"(test.wgsl:5:17 warning: value nan cannot be represented as 'f32'
  var f : f32 = bitcast<f32>(u);
                ^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_ConvertNotRepresentable) {
    Init(R"(
enable f16;

@compute @workgroup_size(1)
fn main(@builtin(local_invocation_index) idx : u32) {
  var u : u32 = 100000;
  let f : f16 = f16(u);
}
)");

    BindingList bindings;
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));

    EXPECT_EQ(errors_,
              R"(test.wgsl:7:17 warning: value 100000 cannot be represented as 'f16'
  let f : f16 = f16(u);
                ^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_AddOverflow_F32) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = buffer[idx] + 1e38;
}
)");

    auto output = MakeBuffer<float, 4>({0, -3.4e38f, 2.4e38f, 2.5e38f});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<float>(*output, {1e38f, -2.4e38f, 3.4e38f, 0}));

    EXPECT_EQ(
        errors_,
        R"(test.wgsl:6:17 warning: '250000007218949514365393469883371487232.0 + 99999996802856924650656260769173209088.0' cannot be represented as 'f32'
  buffer[idx] = buffer[idx] + 1e38;
                ^^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_DivideByZero_F32) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = 1.f / buffer[idx];
}
)");

    auto output = MakeBuffer<float, 4>({1, 2, 0, 4});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<float>(*output, {1, 0.5, 1, 0.25}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:17 warning: '1.0 / 0.0' cannot be represented as 'f32'
  buffer[idx] = 1.f / buffer[idx];
                ^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_DivideByZero_U32) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<u32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = 100 / buffer[idx];
}
)");

    auto output = MakeBuffer<uint32_t, 4>({1, 2, 0, 4});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<uint32_t>(*output, {100, 50, 100, 25}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:17 warning: '100 / 0' cannot be represented as 'u32'
  buffer[idx] = 100 / buffer[idx];
                ^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_Sqrt_Negative) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = sqrt(buffer[idx]);
}
)");

    auto output = MakeBuffer<float, 4>({0, 1, -1, 1024});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<float>(*output, {0, 1, 0, 32}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:17 warning: sqrt must be called with a value >= 0
  buffer[idx] = sqrt(buffer[idx]);
                ^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_Acos_OutOfRange) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = acos(buffer[idx]);
}
)");

    auto output = MakeBuffer<float, 4>({-1.f, 0.f, 1.f, 1.1f});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<float>(*output, {3.14159265f, 3.14159265f / 2.f, 0, 0}));

    EXPECT_EQ(
        errors_,
        R"(test.wgsl:6:17 warning: acos must be called with a value in the range [-1 .. 1] (inclusive)
  buffer[idx] = acos(buffer[idx]);
                ^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_Normalize_ZeroLength) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<vec2<f32>>;

@compute @workgroup_size(2)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = normalize(buffer[idx]);
}
)");

    auto output = MakeBuffer<float, 4>({0.0001f, 0.f, 0.f, 0.f});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<float>(*output, {1.f, 0, 0, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:17 warning: zero length vector can not be normalized
  buffer[idx] = normalize(buffer[idx]);
                ^^^^^^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_QuantizeToF16_TooLarge) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : array<f32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  buffer[idx] = quantizeToF16(buffer[idx]);
}
)");

    auto output = MakeBuffer<float, 4>({0.f, 1.f, 65504.f, 65505.f});

    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(output.get(), 0, output->Size());
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));
    EXPECT_TRUE(CheckEqual<float>(*output, {0.f, 1.f, 65504.f, 0}));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:17 warning: value 65505.0 cannot be represented as 'f16'
  buffer[idx] = quantizeToF16(buffer[idx]);
                ^^^^^^^^^^^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, ConstEvalError_NoCascade) {
    // Make sure an invalid value only causes an error when first created.
    Init(R"(
@compute @workgroup_size(1)
fn main() {
  var a = 1.f;
  var b = 0.f;
  var c = a / b;
  var d = c * 2;
  var e = c + c;
  var f = sqrt(c);
}
)");

    BindingList bindings;
    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), std::move(bindings), true));

    EXPECT_EQ(errors_,
              R"(test.wgsl:6:11 warning: '1.0 / 0.0' cannot be represented as 'f32'
  var c = a / b;
          ^^^^^

)");
}

TEST_F(ComputeEndToEndTest, NonUniformBarrier_OneBarrier_SomeFinished) {
    Init(R"(
enable chromium_disable_uniformity_analysis;

@compute @workgroup_size(64)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx % 4 != 0) {
    workgroupBarrier();
  }
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(error: barrier not reached by all invocations in the workgroup

test.wgsl:7:5 note: invocation(1,0,0) and 47 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

note: 16 invocations have finished running the shader

)");
}

TEST_F(ComputeEndToEndTest, NonUniformBarrier_TwoBarriers) {
    Init(R"(
enable chromium_disable_uniformity_analysis;

@compute @workgroup_size(64)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx % 4 != 0) {
    workgroupBarrier();
  } else {
    workgroupBarrier();
  }
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(error: barrier not reached by all invocations in the workgroup

test.wgsl:9:5 note: invocation(0,0,0) and 15 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test.wgsl:7:5 note: invocation(1,0,0) and 47 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

)");
}

TEST_F(ComputeEndToEndTest, NonUniformBarrier_TwoBarriers_SomeFinished) {
    Init(R"(
enable chromium_disable_uniformity_analysis;

@compute @workgroup_size(64)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx % 4 != 0) {
    workgroupBarrier();
  } else if (idx > 10) {
    workgroupBarrier();
  }
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(error: barrier not reached by all invocations in the workgroup

test.wgsl:7:5 note: invocation(1,0,0) and 47 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test.wgsl:9:5 note: invocation(12,0,0) and 12 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

note: 3 invocations have finished running the shader

)");
}

TEST_F(ComputeEndToEndTest, NonUniformBarrier_ThreeBarriers) {
    Init(R"(
enable chromium_disable_uniformity_analysis;

@compute @workgroup_size(64)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx % 4 != 0) {
    workgroupBarrier();
  } else if (idx > 10) {
    workgroupBarrier();
  } else {
    workgroupBarrier();
  }
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(error: barrier not reached by all invocations in the workgroup

test.wgsl:11:5 note: invocation(0,0,0) and 2 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test.wgsl:7:5 note: invocation(1,0,0) and 47 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

note: 13 invocations are waiting at other barriers

)");
}

TEST_F(ComputeEndToEndTest, NonUniformBarrier_ThreeBarriers_SomeFinished) {
    Init(R"(
enable chromium_disable_uniformity_analysis;

@compute @workgroup_size(64)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx % 4 != 0) {
    workgroupBarrier();
  } else if (idx > 10) {
    workgroupBarrier();
  } else if (idx == 8) {
    workgroupBarrier();
  }
}
)");

    EXPECT_TRUE(RunShader(UVec3(1, 1, 1), {}, true));
    EXPECT_EQ(errors_,
              R"(error: barrier not reached by all invocations in the workgroup

test.wgsl:7:5 note: invocation(1,0,0) and 47 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

test.wgsl:11:5 note: invocation(8,0,0) and 0 other invocations waiting here
    workgroupBarrier();
    ^^^^^^^^^^^^^^^^

note: 13 invocations are waiting at other barriers

note: 2 invocations have finished running the shader

)");
}

}  // namespace
}  // namespace tint::interp
