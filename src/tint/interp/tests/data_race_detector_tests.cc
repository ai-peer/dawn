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

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "tint/interp/data_race_detector.h"
#include "tint/interp/memory.h"
#include "tint/interp/shader_executor.h"
#include "tint/tint.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {
namespace {

class DataRaceDetectorTest : public testing::Test {
  protected:
    // Initialize the test with a WGSL source string.
    void Init(std::string source) {
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

        // Create the shader executor and a data race detector.
        auto result = ShaderExecutor::Create(*program_, "main", {});
        ASSERT_EQ(result, Success) << result.Failure();
        executor_ = result.Move();
        data_race_detector_ = std::make_unique<DataRaceDetector>(*executor_);
        executor_->AddErrorCallback([&](auto&& error) {
            errors_ += "\n" + error.Str();
            error_count_++;
        });
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

    std::unique_ptr<Source::File> file_;
    std::unique_ptr<Program> program_;
    std::unique_ptr<ShaderExecutor> executor_;
    std::unique_ptr<DataRaceDetector> data_race_detector_;
    std::string errors_;
    uint32_t error_count_ = 0;
};

TEST_F(DataRaceDetectorTest, Workgroup_ReadRead) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    let x = wgvar;
  } else {
    let y = wgvar;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, Workgroup_ReadWrite_WithBarrier) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = wgvar;
  workgroupBarrier();
  if (idx == 0) {
    wgvar = 42;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, Workgroup_ReadWrite_WithoutBarrier) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = wgvar;
  if (idx == 0) {
    wgvar = 42;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:8:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:6:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = wgvar;
          ^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Workgroup_WriteRead_WithBarrier) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = 42;
  }
  workgroupBarrier();
  let x = wgvar;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, Workgroup_WriteRead_WithoutBarrier) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = 42;
  }
  let x = wgvar;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:7:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:9:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = wgvar;
          ^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Workgroup_WriteWrite_WithBarrier) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = 42;
  }
  workgroupBarrier();
  if (idx == 1) {
    wgvar = 99;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, Workgroup_WriteWrite_WithoutBarrier) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = 42;
  }
  if (idx == 1) {
    wgvar = 99;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:7:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:10:11 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    wgvar = 99;
          ^
)");
}

TEST_F(DataRaceDetectorTest, WorkgroupUniformLoad) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = 42;
  }
  let x = workgroupUniformLoad(&wgvar);
  if (idx == 0) {
    wgvar = 99;
  }
  let y = workgroupUniformLoad(&wgvar);
  let z = wgvar;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, Workgroup_WrongBarrier) {
    // A storageBarrier() should not synchronize accesses to workgroup memory.
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = wgvar;
  storageBarrier();
  if (idx == 0) {
    wgvar = 42;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:9:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:6:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = wgvar;
          ^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Storage_ReadRead) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    let x = buffer;
  } else {
    let y = buffer;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({2, 1, 1}, std::move(bindings)));
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_ReadWrite_WithBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = buffer;
  storageBarrier();
  if (idx == 0) {
    buffer = 42;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings)));
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_ReadWrite_WithoutBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = buffer;
  if (idx == 0) {
    buffer = 42;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:8:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:6:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = buffer;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_WriteRead_WithBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    buffer = 42;
  }
  storageBarrier();
  let x = buffer;
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings)));
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_WriteRead_WithoutBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    buffer = 42;
  }
  let x = buffer;
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:7:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:9:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = buffer;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_WriteWrite_WithBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    buffer = 42;
  }
  storageBarrier();
  if (idx == 1) {
    buffer = 99;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings)));
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_WriteWrite_WithoutBarrier) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    buffer = 42;
  }
  if (idx == 1) {
    buffer = 99;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:7:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:10:12 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    buffer = 99;
           ^
)");
}

TEST_F(DataRaceDetectorTest, Storage_IntraGroup_WrongBarrier) {
    // A workgroupBarrier() should not synchronize accesses to storage buffer memory.
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = buffer;
  workgroupBarrier();
  if (idx == 0) {
    buffer = 42;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({1, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:9:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:6:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = buffer;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Storage_InterGroup_ReadWrite) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(1)
fn main(@builtin(workgroup_id) group : vec3<u32>) {
  let x = buffer;
  if (group.x == 0) {
    buffer = 42;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({2, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:8:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:6:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(1,0,0)
  let x = buffer;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Storage_InterGroup_WriteRead) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(1)
fn main(@builtin(workgroup_id) group : vec3<u32>) {
  if (group.x == 0) {
    buffer = 42;
  }
  let x = buffer;
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({2, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:7:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:9:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(1,0,0)
  let x = buffer;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Storage_InterGroup_WriteWrite) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(1)
fn main(@builtin(workgroup_id) group : vec3<u32>) {
  if (group.x == 0) {
    buffer = 42;
  }
  if (group.x == 1) {
    buffer = 99;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({2, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:7:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:10:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(1,0,0)
    buffer = 99;
           ^
)");
}

TEST_F(DataRaceDetectorTest, Storage_InterGroup_WithBarrier) {
    // A storageBarrier should not synchronize across workgroups.
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(1)
fn main(@builtin(workgroup_id) group : vec3<u32>) {
  let x = buffer;
  storageBarrier();
  if (group.x == 0) {
    buffer = 42;
  }
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({2, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:9:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:6:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(1,0,0)
  let x = buffer;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, WriteWrite_SameLocation) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  wgvar = idx;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:6:9 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
  wgvar = idx;
        ^

test.wgsl:6:9 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  wgvar = idx;
        ^
)");
}

TEST_F(DataRaceDetectorTest, Struct_ReadWrite) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
  c : i32,
  d : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = wgvar;
  if (idx == 3) {
    wgvar = S();
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:9:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : S;
               ^^^^^

test.wgsl:15:11 note: stored 16 bytes at offset 0
while running local_invocation_id(3,0,0) workgroup_id(0,0,0)
    wgvar = S();
          ^

test.wgsl:13:11 note: loaded 16 bytes at offset 0
while running local_invocation_id(2,0,0) workgroup_id(0,0,0)
  let x = wgvar;
          ^^^^^
)");
}

TEST_F(DataRaceDetectorTest, Struct_ReadWrite_DifferentMembers) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = wgvar.a;
  if (idx == 3) {
    wgvar.b = 42;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, ReadStruct_WriteMember) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let x = wgvar;
  if (idx == 3) {
    wgvar.b = 42;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:7:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : S;
               ^^^^^

test.wgsl:13:13 note: stored 4 bytes at offset 4
while running local_invocation_id(3,0,0) workgroup_id(0,0,0)
    wgvar.b = 42;
            ^

test.wgsl:11:11 note: loaded 8 bytes at offset 0
while running local_invocation_id(2,0,0) workgroup_id(0,0,0)
  let x = wgvar;
          ^^^^^
)");
}

TEST_F(DataRaceDetectorTest, ReadMember_WriteStruct) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
    let x = wgvar.b;
  if (idx == 3) {
    wgvar = S();
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:7:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : S;
               ^^^^^

test.wgsl:13:11 note: stored 8 bytes at offset 0
while running local_invocation_id(3,0,0) workgroup_id(0,0,0)
    wgvar = S();
          ^

test.wgsl:11:13 note: loaded 4 bytes at offset 4
while running local_invocation_id(2,0,0) workgroup_id(0,0,0)
    let x = wgvar.b;
            ^^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, WriteStruct_ReadMember) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = S();
  } else {
    let x = wgvar.b;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:7:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : S;
               ^^^^^

test.wgsl:12:11 note: stored 8 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = S();
          ^

test.wgsl:14:13 note: loaded 4 bytes at offset 4
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    let x = wgvar.b;
            ^^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, WriteMember_ReadStruct) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar.b = 42;
  } else {
    let x = wgvar;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:7:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : S;
               ^^^^^

test.wgsl:12:13 note: stored 4 bytes at offset 4
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar.b = 42;
            ^

test.wgsl:14:13 note: loaded 8 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    let x = wgvar;
            ^^^^^
)");
}

TEST_F(DataRaceDetectorTest, WriteMember_WriteStruct) {
    Init(R"(
struct S {
  a : i32,
  b : i32,
}

var<workgroup> wgvar : S;

@compute @workgroup_size(2)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar.b = 42;
  } else {
    wgvar = S();
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:7:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : S;
               ^^^^^

test.wgsl:12:13 note: stored 4 bytes at offset 4
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar.b = 42;
            ^

test.wgsl:14:11 note: stored 8 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    wgvar = S();
          ^
)");
}

TEST_F(DataRaceDetectorTest, VectorComponentWrite) {
    Init(R"(
var<workgroup> wgvar : vec4<i32>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  wgvar[idx] = 42;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : vec4<i32>;
               ^^^^^

test.wgsl:6:14 note: stored 16 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
  wgvar[idx] = 42;
             ^

test.wgsl:6:14 note: stored 16 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  wgvar[idx] = 42;
             ^

note: writing to a component of a vector may write to every component of that vector)");
}

TEST_F(DataRaceDetectorTest, ReadAfterWriteAcrossLoopIterations) {
    Init(R"(
const wgsize = 2;
var<workgroup> wgvar : array<u32, wgsize>;

@compute @workgroup_size(wgsize)
fn main(@builtin(local_invocation_index) idx : u32) {
  var sum = 0u;
  for (var i = 0u; i < 4; i+=wgsize) {
    wgvar[idx] = idx + i;
    workgroupBarrier();
    for (var j = 0; j < wgsize; j++) {
      sum += wgvar[j];
    }
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(error_count_, 1u);
    EXPECT_THAT(errors_, testing::HasSubstr(
                             R"(
test.wgsl:3:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : array<u32, wgsize>;
               ^^^^^

test.wgsl:9:16 note: stored 4 bytes at offset)"));
    EXPECT_THAT(errors_, testing::HasSubstr(R"(workgroup_id(0,0,0)
    wgvar[idx] = idx + i;
               ^

test.wgsl:12:14 note: loaded 4 bytes at offset)"));
    EXPECT_THAT(errors_, testing::HasSubstr(R"(workgroup_id(0,0,0)
      sum += wgvar[j];
             ^^^^^^^^
)"));
}

TEST_F(DataRaceDetectorTest, MultipleRaces_DifferentWorkgroupVars) {
    Init(R"(
var<workgroup> wgvar1 : u32;
var<workgroup> wgvar2 : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar1 = 42;
  }

  if (idx == 1) {
    wgvar2 = wgvar1 + 10;
  }

  let x = wgvar2;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(error_count_, 2u);
    EXPECT_THAT(errors_, testing::HasSubstr(R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar1 : u32;
               ^^^^^^

test.wgsl:8:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar1 = 42;
           ^

test.wgsl:12:14 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    wgvar2 = wgvar1 + 10;
             ^^^^^^)"));
    EXPECT_THAT(errors_, testing::HasSubstr(R"(
test.wgsl:3:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar2 : u32;
               ^^^^^^

test.wgsl:12:12 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    wgvar2 = wgvar1 + 10;
           ^

test.wgsl:15:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
  let x = wgvar2;
          ^^^^^^)"));
}

TEST_F(DataRaceDetectorTest, MultipleRaces_SameWorkgroupVar_DifferentOffset) {
    Init(R"(
var<workgroup> wgvar : array<u32, 2>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  for (var i = 0; i < 2; i++) {
    if (idx == 0) {
      wgvar[i] = 42;
    }
    let x = wgvar[i] + 10;
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_EQ(error_count_, 1u);
    EXPECT_THAT(errors_, testing::HasSubstr(
                             R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : array<u32, 2>;
               ^^^^^

test.wgsl:8:16 note: stored 4 bytes at offset)"));
    EXPECT_THAT(errors_, testing::HasSubstr(R"(workgroup_id(0,0,0)
      wgvar[i] = 42;
               ^

test.wgsl:10:13 note: loaded 4 bytes at offset)"));
    EXPECT_THAT(errors_, testing::HasSubstr(R"(workgroup_id(0,0,0)
    let x = wgvar[i] + 10;
            ^^^^^^^^
)"));
}

TEST_F(DataRaceDetectorTest, MultipleRaces_SameWorkgroupVar_SameOffset_DifferentLocations) {
    Init(R"(
var<workgroup> wgvar : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar = 42;
  }
  let x = wgvar + 10;

  workgroupBarrier();

  if (idx == 0) {
    wgvar = 42;
  }
  let y = wgvar + 10;
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:14:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:16:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let y = wgvar + 10;
          ^^^^^
)"));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:7:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:9:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = wgvar + 10;
          ^^^^^
)"));
}

TEST_F(DataRaceDetectorTest, MultipleRaces_SameWorkgroupVar_DifferentGroups) {
    Init(R"(
var<workgroup> wgvar1 : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  if (idx == 0) {
    wgvar1 = 42;
  }

  let x = wgvar1 + 10;
}
)");
    EXPECT_TRUE(RunShader({4, 1, 1}, {}, true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar1 : u32;
               ^^^^^^

test.wgsl:7:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar1 = 42;
           ^

test.wgsl:10:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  let x = wgvar1 + 10;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, MultipleRaces_SameStorageBuffer_DifferentGroups) {
    Init(R"(
@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(1)
fn main(@builtin(workgroup_id) id : vec3<u32>) {
  if (id.x == 0) {
    buffer = 42;
  }

  let x = buffer + 10;
}
)");
    auto buffer = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    EXPECT_TRUE(RunShader({4, 1, 1}, std::move(bindings), true));
    EXPECT_EQ(errors_,
              R"(
test.wgsl:2:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:7:12 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    buffer = 42;
           ^

test.wgsl:10:11 note: loaded 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(1,0,0)
  let x = buffer + 10;
          ^^^^^^
)");
}

TEST_F(DataRaceDetectorTest, MultipleRaces_WorkgroupAndStorageBuffer) {
    Init(R"(
var<workgroup> wgvar : u32;

@group(0) @binding(0) var<storage, read_write> buffer : u32;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  let tmp = buffer;
  if (idx == 0) {
    wgvar = 42;
  }

  if (idx == 1) {
    // Intra-group read-write race on wgvar
    // Intra-group read-write race on buffer
    // Inter-group read-write race on buffer (pruned)
    // Inter-group write-write race on buffer
    buffer = wgvar;
  }
}
)");
    auto buffer1 = MakeBuffer<uint32_t>({});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(buffer1.get(), 0, buffer1->Size());
    EXPECT_TRUE(RunShader({4, 1, 1}, std::move(bindings), true));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:4:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:18:12 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    buffer = wgvar;
           ^

test.wgsl:8:13 note: loaded 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
  let tmp = buffer;
            ^^^^^^
)"));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:2:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar : u32;
               ^^^^^

test.wgsl:10:11 note: stored 4 bytes at offset 0
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    wgvar = 42;
          ^

test.wgsl:18:14 note: loaded 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    buffer = wgvar;
             ^^^^^
)"));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:4:48 warning: data race detected on accesses to storage buffer
@group(0) @binding(0) var<storage, read_write> buffer : u32;
                                               ^^^^^^

test.wgsl:18:12 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
    buffer = wgvar;
           ^

test.wgsl:18:12 note: stored 4 bytes at offset 0
while running local_invocation_id(1,0,0) workgroup_id(1,0,0)
    buffer = wgvar;
           ^
)"));
}

TEST_F(DataRaceDetectorTest, SubWord_WithoutRaces) {
    Init(R"(
enable f16;
var<workgroup> wgvar1 : array<f16, 4>;
var<workgroup> wgvar2 : array<bool, 4>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  wgvar1[idx] = f16(idx);
  wgvar2[idx] = bool(idx);
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}));
}

TEST_F(DataRaceDetectorTest, SubWord_WithRaces) {
    Init(R"(
enable f16;
var<workgroup> wgvar1 : array<f16, 4>;
var<workgroup> wgvar2 : array<bool, 4>;

@compute @workgroup_size(4)
fn main(@builtin(local_invocation_index) idx : u32) {
  wgvar1[idx] = f16(idx);
  wgvar2[idx] = bool(idx);
  if (idx == 0) {
    if (!wgvar2[1]) {
      let x = wgvar1[2];
    }
  }
}
)");
    EXPECT_TRUE(RunShader({1, 1, 1}, {}, true));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:3:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar1 : array<f16, 4>;
               ^^^^^^

test.wgsl:8:15 note: stored 2 bytes at offset 4
while running local_invocation_id(2,0,0) workgroup_id(0,0,0)
  wgvar1[idx] = f16(idx);
              ^

test.wgsl:12:15 note: loaded 2 bytes at offset 4
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
      let x = wgvar1[2];
              ^^^^^^^^^
)"));
    EXPECT_THAT(errors_,
                testing::HasSubstr(
                    R"(test.wgsl:4:16 warning: data race detected on accesses to workgroup variable
var<workgroup> wgvar2 : array<bool, 4>;
               ^^^^^^

test.wgsl:9:15 note: stored 4 bytes at offset 4
while running local_invocation_id(1,0,0) workgroup_id(0,0,0)
  wgvar2[idx] = bool(idx);
              ^

test.wgsl:11:10 note: loaded 4 bytes at offset 4
while running local_invocation_id(0,0,0) workgroup_id(0,0,0)
    if (!wgvar2[1]) {
         ^^^^^^^^^
)"));
}

}  // namespace
}  // namespace tint::interp
