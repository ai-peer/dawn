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
#include <sstream>

#include "gtest/gtest.h"
#include "tint/interp/interactive_debugger.h"
#include "tint/interp/memory.h"
#include "tint/interp/shader_executor.h"
#include "tint/tint.h"
#include "tint/utils/text/styled_text_printer.h"

namespace tint::interp {
namespace {

class InteractiveDebuggerTest : public testing::Test {
  protected:
    // Initialize the test with a WGSL source string.
    void Run(UVec3 group_count,
             BindingList&& bindings,
             std::string source,
             std::string input,
             std::string expected_output,
             std::string expected_error) {
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

        std::istringstream iss(input);

        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();

        // Create the shader executor and an interactive debugger.
        auto result = ShaderExecutor::Create(*program_, "main", {});
        ASSERT_EQ(result, Success) << result.Failure();
        executor_ = result.Move();
        debugger_ = std::make_unique<InteractiveDebugger>(*executor_, iss);

        // Run the shader.
        auto success = executor_->Run(group_count, std::move(bindings));
        auto output = testing::internal::GetCapturedStdout();
        auto error = testing::internal::GetCapturedStderr();
        EXPECT_EQ(success, Success);
        EXPECT_EQ(output, expected_output);
        EXPECT_EQ(error, expected_error);
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
    std::unique_ptr<InteractiveDebugger> debugger_;
};

TEST_F(InteractiveDebuggerTest, Basic) {
    Run(UVec3(1, 1, 1), {}, R"(
@compute @workgroup_size(1)
fn main() {}
)",
        R"(
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:3:12
   1:
   2: @compute @workgroup_size(1)
-> 3: fn main() {}
                 ^
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, StepAndPrint) {
    Run(UVec3(1, 1, 1), {}, R"(
var<private> a : i32;

fn foo(a : i32) -> i32 {
  return a + 10;
}

@compute @workgroup_size(1)
fn main() {
  a++;
  a++;
  a = foo(10 + a);
}
)",
        R"(
print a
step
p a
s
p a
s
p a
s
p a
s
p a
c
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:10:3
    7:
    8: @compute @workgroup_size(1)
    9: fn main() {
-> 10:   a++;
         ^
   11:   a++;
   12:   a = foo(10 + a);
   13: }
a = 0
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:11:3
    8: @compute @workgroup_size(1)
    9: fn main() {
   10:   a++;
-> 11:   a++;
         ^
   12:   a = foo(10 + a);
   13: }
a = 1
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:3
    9: fn main() {
   10:   a++;
   11:   a++;
-> 12:   a = foo(10 + a);
         ^
   13: }
a = 2
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: foo() at test.wgsl:5:10
    2: var<private> a : i32;
    3:
    4: fn foo(a : i32) -> i32 {
->  5:   return a + 10;
                ^
    6: }
    7:
    8: @compute @workgroup_size(1)
a = 12
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:5
    9: fn main() {
   10:   a++;
   11:   a++;
-> 12:   a = foo(10 + a);
           ^
   13: }
a = 2
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:13:1
   10:   a++;
   11:   a++;
   12:   a = foo(10 + a);
-> 13: }
       ^
a = 22
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, StepExpression) {
    Run(UVec3(1, 1, 1), {}, R"(

@compute @workgroup_size(1)
fn main() {
  var a = array<i32, 4>(1, 2, 3, 4);
  a[a[0]] = a[1] + a[3];
}
)",
        R"(
step
print a
stepe
se
se
se
se
se
se
se
se
se
p a
c
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:5:3
   2:
   3: @compute @workgroup_size(1)
   4: fn main() {
-> 5:   var a = array<i32, 4>(1, 2, 3, 4);
        ^^^^^
   6:   a[a[0]] = a[1] + a[3];
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:3
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
        ^
   7: }
a = array<i32, 4>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
}
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:5
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
          ^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:5
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
          ^^^^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:3
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
        ^^^^^^^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:13
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
                  ^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:13
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
                  ^^^^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:20
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
                         ^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:20
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
                         ^^^^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:13
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
                  ^^^^^^^^^^^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:11
   3: @compute @workgroup_size(1)
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
-> 6:   a[a[0]] = a[1] + a[3];
                ^
   7: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:7:1
   4: fn main() {
   5:   var a = array<i32, 4>(1, 2, 3, 4);
   6:   a[a[0]] = a[1] + a[3];
-> 7: }
      ^
a = array<i32, 4>{
  [0] = 1,
  [1] = 6,
  [2] = 3,
  [3] = 4,
}
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, RepeatCommand) {
    Run(UVec3(1, 1, 1), {}, R"(
@compute @workgroup_size(1)
fn main() {
  let a = 1;
  let b = 2;
  let c = a + b;
  let d = a + b + c;
  let e = a + b + c + d;
  let f = a + b + c + d + e;
  let g = a + b + c + d + e + f;
  let h = a + b + c + d + e + f + g;
}
)",
        R"(
step



stepe






s


continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
    1:
    2: @compute @workgroup_size(1)
    3: fn main() {
->  4:   let a = 1;
         ^^^^^
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:5:3
    2: @compute @workgroup_size(1)
    3: fn main() {
    4:   let a = 1;
->  5:   let b = 2;
         ^^^^^
    6:   let c = a + b;
    7:   let d = a + b + c;
    8:   let e = a + b + c + d;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:11
    3: fn main() {
    4:   let a = 1;
    5:   let b = 2;
->  6:   let c = a + b;
                 ^
    7:   let d = a + b + c;
    8:   let e = a + b + c + d;
    9:   let f = a + b + c + d + e;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:7:11
    4:   let a = 1;
    5:   let b = 2;
    6:   let c = a + b;
->  7:   let d = a + b + c;
                 ^
    8:   let e = a + b + c + d;
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:11
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                 ^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:15
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                     ^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:11
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                 ^^^^^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:19
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                         ^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:11
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                 ^^^^^^^^^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:23
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                             ^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:11
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
                 ^^^^^^^^^^^^^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:3
    5:   let b = 2;
    6:   let c = a + b;
    7:   let d = a + b + c;
->  8:   let e = a + b + c + d;
         ^^^^^
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:9:11
    6:   let c = a + b;
    7:   let d = a + b + c;
    8:   let e = a + b + c + d;
->  9:   let f = a + b + c + d + e;
                 ^
   10:   let g = a + b + c + d + e + f;
   11:   let h = a + b + c + d + e + f + g;
   12: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:10:11
    7:   let d = a + b + c;
    8:   let e = a + b + c + d;
    9:   let f = a + b + c + d + e;
-> 10:   let g = a + b + c + d + e + f;
                 ^
   11:   let h = a + b + c + d + e + f + g;
   12: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:11:11
    8:   let e = a + b + c + d;
    9:   let f = a + b + c + d + e;
   10:   let g = a + b + c + d + e + f;
-> 11:   let h = a + b + c + d + e + f + g;
                 ^
   12: }
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, PrintAtomics) {
    Run(UVec3(1, 1, 1), {}, R"(
var<workgroup> a : atomic<u32>;
var<workgroup> arr : array<atomic<i32>, 4>;

@compute @workgroup_size(1)
fn main() {
  atomicAdd(&a, 42);
  atomicStore(&arr[0], 10);
  atomicStore(&arr[1], -20);
  atomicStore(&arr[2], 30);
  atomicStore(&arr[3], -40);
  atomicMax(&arr[1], atomicLoad(&arr[2]));
}
)",
        R"(
print a
print arr
step
print a
step
step
step
step
print arr
step
print arr
c
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:7:14
    4:
    5: @compute @workgroup_size(1)
    6: fn main() {
->  7:   atomicAdd(&a, 42);
                    ^
    8:   atomicStore(&arr[0], 10);
    9:   atomicStore(&arr[1], -20);
   10:   atomicStore(&arr[2], 30);
a = 0
arr = array<atomic<i32>, 4>{
  [0] = 0,
  [1] = 0,
  [2] = 0,
  [3] = 0,
}
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:16
    5: @compute @workgroup_size(1)
    6: fn main() {
    7:   atomicAdd(&a, 42);
->  8:   atomicStore(&arr[0], 10);
                      ^^^
    9:   atomicStore(&arr[1], -20);
   10:   atomicStore(&arr[2], 30);
   11:   atomicStore(&arr[3], -40);
a = 42
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:9:16
    6: fn main() {
    7:   atomicAdd(&a, 42);
    8:   atomicStore(&arr[0], 10);
->  9:   atomicStore(&arr[1], -20);
                      ^^^
   10:   atomicStore(&arr[2], 30);
   11:   atomicStore(&arr[3], -40);
   12:   atomicMax(&arr[1], atomicLoad(&arr[2]));
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:10:16
    7:   atomicAdd(&a, 42);
    8:   atomicStore(&arr[0], 10);
    9:   atomicStore(&arr[1], -20);
-> 10:   atomicStore(&arr[2], 30);
                      ^^^
   11:   atomicStore(&arr[3], -40);
   12:   atomicMax(&arr[1], atomicLoad(&arr[2]));
   13: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:11:16
    8:   atomicStore(&arr[0], 10);
    9:   atomicStore(&arr[1], -20);
   10:   atomicStore(&arr[2], 30);
-> 11:   atomicStore(&arr[3], -40);
                      ^^^
   12:   atomicMax(&arr[1], atomicLoad(&arr[2]));
   13: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:14
    9:   atomicStore(&arr[1], -20);
   10:   atomicStore(&arr[2], 30);
   11:   atomicStore(&arr[3], -40);
-> 12:   atomicMax(&arr[1], atomicLoad(&arr[2]));
                    ^^^
   13: }
arr = array<atomic<i32>, 4>{
  [0] = 10,
  [1] = -20,
  [2] = 30,
  [3] = -40,
}
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:13:1
   10:   atomicStore(&arr[2], 30);
   11:   atomicStore(&arr[3], -40);
   12:   atomicMax(&arr[1], atomicLoad(&arr[2]));
-> 13: }
       ^
arr = array<atomic<i32>, 4>{
  [0] = 10,
  [1] = 30,
  [2] = 30,
  [3] = -40,
}
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, PrintRuntimeArray) {
    auto arr = MakeBuffer<int32_t, 4>({1, 2, 3, 4});
    auto buffer = MakeBuffer<int32_t, 8>({42, 99, 10, -20, 30, -40, 50, -60});
    BindingList bindings;
    bindings[{0, 0}] = Binding::MakeBufferBinding(arr.get(), 0, arr->Size());
    bindings[{0, 1}] = Binding::MakeBufferBinding(buffer.get(), 0, buffer->Size());
    Run(UVec3(1, 1, 1), std::move(bindings), R"(
struct S {
  a : i32,
  b : i32,
  data : array<i32>,
}
@group(0) @binding(0) var<storage, read_write> arr : array<i32>;
@group(0) @binding(1) var<storage, read_write> buffer : S;

@compute @workgroup_size(1)
fn main() {
  _ = arr[0];
  _ = buffer.data[0];
}
)",
        R"(
print arr
print buffer
c
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:3
    9:
   10: @compute @workgroup_size(1)
   11: fn main() {
-> 12:   _ = arr[0];
         ^
   13:   _ = buffer.data[0];
   14: }
arr = array<i32>{
  [0] = 1,
  [1] = 2,
  [2] = 3,
  [3] = 4,
}
buffer = S{
  .a = 42,
  .b = 99,
  .data = array<i32>{
    [0] = 10,
    [1] = -20,
    [2] = 30,
    [3] = -40,
    [4] = 50,
    [5] = -60,
  },
}
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, PrintArrayWithOverridableCount) {
    Run(UVec3(1, 1, 1), {}, R"(
override size : i32 = 3;
var<workgroup> arr1 : array<u32, size>;
var<workgroup> arr2 : array<u32, 2 * size>;

@compute @workgroup_size(3)
fn main(@builtin(local_invocation_index) idx : u32) {
  arr1[idx] = idx;
  arr2[idx] = idx;
  arr2[3 + idx] = idx * 2 + 1;
  workgroupBarrier();
  _ = 0;
}
)",
        R"(
break 12
continue
print size
print arr1
print arr2
c
c
c
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:3
    5:
    6: @compute @workgroup_size(3)
    7: fn main(@builtin(local_invocation_index) idx : u32) {
->  8:   arr1[idx] = idx;
         ^^^^
    9:   arr2[idx] = idx;
   10:   arr2[3 + idx] = idx * 2 + 1;
   11:   workgroupBarrier();
Breakpoint added at test.wgsl:12
-> 12:   _ = 0;
         ^
Hit breakpoint on line 12
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:3
    9:   arr2[idx] = idx;
   10:   arr2[3 + idx] = idx * 2 + 1;
   11:   workgroupBarrier();
-> 12:   _ = 0;
         ^
   13: }
size = 3
arr1 = array<u32, size>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
}
arr2 = array<u32, [unnamed override-expression]>{
  [0] = 0,
  [1] = 1,
  [2] = 2,
  [3] = 1,
  [4] = 3,
  [5] = 5,
}
Hit breakpoint on line 12
* workgroup_id(0,0,0)
  * local_invocation_id(1,0,0)
    * frame #0: main() at test.wgsl:12:3
    9:   arr2[idx] = idx;
   10:   arr2[3 + idx] = idx * 2 + 1;
   11:   workgroupBarrier();
-> 12:   _ = 0;
         ^
   13: }
Hit breakpoint on line 12
* workgroup_id(0,0,0)
  * local_invocation_id(2,0,0)
    * frame #0: main() at test.wgsl:12:3
    9:   arr2[idx] = idx;
   10:   arr2[3 + idx] = idx * 2 + 1;
   11:   workgroupBarrier();
-> 12:   _ = 0;
         ^
   13: }
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, AutoBreakOnError) {
    Run(UVec3(1, 1, 1), {}, R"(
@compute @workgroup_size(1)
fn main() {
  var a = 1.5;
  var b = 0.0;
  var c = a / b;
  b = 2.0;
}
)",
        R"(
continue
print b
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(1)
   3: fn main() {
-> 4:   var a = 1.5;
        ^^^^^
   5:   var b = 0.0;
   6:   var c = a / b;
   7:   b = 2.0;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:11
   3: fn main() {
   4:   var a = 1.5;
   5:   var b = 0.0;
-> 6:   var c = a / b;
                ^^^^^
   7:   b = 2.0;
   8: }
b = 0.000000
)",
        R"(test.wgsl:6:11 warning: '1.5 / 0.0' cannot be represented as 'f32'
  var c = a / b;
          ^^^^^

)");
}

TEST_F(InteractiveDebuggerTest, Backtrace) {
    Run(UVec3(1, 1, 1), {}, R"(
fn zoo() {
   let y = 42;
}

fn bar(x : i32) -> i32 {
  zoo();
  return x + 1;
}

fn foo(x : i32) -> i32 {
  return bar(x) + 1;
}

@compute @workgroup_size(2)
fn main() {
  let a = foo(0);
}
)",
        R"(
backtrace
backtrace 100
step
step
step
bt
bt 1
bt 2
bt 3
bt 4
bt 100
bt 1 2
bt a
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:17:11
   14:
   15: @compute @workgroup_size(2)
   16: fn main() {
-> 17:   let a = foo(0);
                 ^^^^^^
   18: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:17:11
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:17:11
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: foo() at test.wgsl:12:14
    9: }
   10:
   11: fn foo(x : i32) -> i32 {
-> 12:   return bar(x) + 1;
                    ^
   13: }
   14:
   15: @compute @workgroup_size(2)
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: bar() at test.wgsl:7:3
    4: }
    5:
    6: fn bar(x : i32) -> i32 {
->  7:   zoo();
         ^^^
    8:   return x + 1;
    9: }
   10:
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
    1:
    2: fn zoo() {
->  3:    let y = 42;
          ^^^^^
    4: }
    5:
    6: fn bar(x : i32) -> i32 {
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
      frame #1: bar() at test.wgsl:7:3
      frame #2: foo() at test.wgsl:12:10
      frame #3: main() at test.wgsl:17:11
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
      frame #1: bar() at test.wgsl:7:3
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
      frame #1: bar() at test.wgsl:7:3
      frame #2: foo() at test.wgsl:12:10
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
      frame #1: bar() at test.wgsl:7:3
      frame #2: foo() at test.wgsl:12:10
      frame #3: main() at test.wgsl:17:11
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: zoo() at test.wgsl:3:4
      frame #1: bar() at test.wgsl:7:3
      frame #2: foo() at test.wgsl:12:10
      frame #3: main() at test.wgsl:17:11
)",
        R"(Expected 'backtrace [max_depth]'
Invalid maximum depth value 'a'
)");
}

TEST_F(InteractiveDebuggerTest, Breakpoints) {
    Run(UVec3(1, 1, 1), {}, R"(
fn foo(x : i32) -> i32 {
  return x + 1;    // break 3
}

@compute @workgroup_size(2)
fn main() {
  var a = 0;
  var b = foo(0);  // break 9
  var c = foo(a);  // break 10
  var d =          // break 11
    b + c          // break 12
    + foo(0)
  ;
}
)",
        R"(
break 3
break 9
b 10
b 11
b 12
continue
continue
continue
continue
continue
continue
continue
breakpoint clear 9
breakpoint clear 10
br clear 11
br clear 3
br list
continue
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:3
    5:
    6: @compute @workgroup_size(2)
    7: fn main() {
->  8:   var a = 0;
         ^^^^^
    9:   var b = foo(0);  // break 9
   10:   var c = foo(a);  // break 10
   11:   var d =          // break 11
Breakpoint added at test.wgsl:3
->  3:   return x + 1;    // break 3
                ^^^^^
Breakpoint added at test.wgsl:9
->  9:   var b = foo(0);  // break 9
                 ^^^^^^
Breakpoint added at test.wgsl:10
-> 10:   var c = foo(a);  // break 10
                 ^^^^^^
Breakpoint added at test.wgsl:11
-> 11:   var d =          // break 11
         ^^^^^
Breakpoint added at test.wgsl:12
-> 12:     b + c          // break 12
           ^^^^^
Hit breakpoint on line 9
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:9:11
    6: @compute @workgroup_size(2)
    7: fn main() {
    8:   var a = 0;
->  9:   var b = foo(0);  // break 9
                 ^^^^^^
   10:   var c = foo(a);  // break 10
   11:   var d =          // break 11
   12:     b + c          // break 12
Hit breakpoint on line 3
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: foo() at test.wgsl:3:10
    1:
    2: fn foo(x : i32) -> i32 {
->  3:   return x + 1;    // break 3
                ^^^^^
    4: }
    5:
    6: @compute @workgroup_size(2)
Hit breakpoint on line 10
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:10:11
    7: fn main() {
    8:   var a = 0;
    9:   var b = foo(0);  // break 9
-> 10:   var c = foo(a);  // break 10
                 ^^^^^^
   11:   var d =          // break 11
   12:     b + c          // break 12
   13:     + foo(0)
Hit breakpoint on line 3
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: foo() at test.wgsl:3:10
    1:
    2: fn foo(x : i32) -> i32 {
->  3:   return x + 1;    // break 3
                ^^^^^
    4: }
    5:
    6: @compute @workgroup_size(2)
Hit breakpoint on line 12
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:5
    9:   var b = foo(0);  // break 9
   10:   var c = foo(a);  // break 10
   11:   var d =          // break 11
-> 12:     b + c          // break 12
           ^^^^^
   13:     + foo(0)
   14:   ;
   15: }
Hit breakpoint on line 3
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: foo() at test.wgsl:3:10
    1:
    2: fn foo(x : i32) -> i32 {
->  3:   return x + 1;    // break 3
                ^^^^^
    4: }
    5:
    6: @compute @workgroup_size(2)
Hit breakpoint on line 11
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:11:3
    8:   var a = 0;
    9:   var b = foo(0);  // break 9
   10:   var c = foo(a);  // break 10
-> 11:   var d =          // break 11
         ^^^^^
   12:     b + c          // break 12
   13:     + foo(0)
   14:   ;
Breakpoint removed at test.wgsl:9
Breakpoint removed at test.wgsl:10
Breakpoint removed at test.wgsl:11
Breakpoint removed at test.wgsl:3
Existing breakpoints:
-> 12:     b + c          // break 12
           ^^^^^
Hit breakpoint on line 12
* workgroup_id(0,0,0)
  * local_invocation_id(1,0,0)
    * frame #0: main() at test.wgsl:12:5
    9:   var b = foo(0);  // break 9
   10:   var c = foo(a);  // break 10
   11:   var d =          // break 11
-> 12:     b + c          // break 12
           ^^^^^
   13:     + foo(0)
   14:   ;
   15: }
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, Breakpoints_Invalid) {
    Run(UVec3(1, 1, 1), {}, R"(
@compute @workgroup_size(1)
fn main() {
  var a : i32;

}
)",
        R"(
break
break 4 a
break 0
break 100
break a
break 4a
break 3
break 5
break 4
break 4
breakpoint
breakpoint foo
breakpoint clear
breakpoint clear foo
breakpoint clear 100
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(1)
   3: fn main() {
-> 4:   var a : i32;
        ^^^^^^^^^^^
   5:
   6: }
Breakpoint added at test.wgsl:4
-> 4:   var a : i32;
        ^^^^^^^^^^^
breakpoint list           List existing breakpoints
breakpoint clear <line>   Delete a breakpoint from the specified line
breakpoint list           List existing breakpoints
breakpoint clear <line>   Delete a breakpoint from the specified line
breakpoint list           List existing breakpoints
breakpoint clear <line>   Delete a breakpoint from the specified line
)",
        R"(Expected 'break <line_number>'
Expected 'break <line_number>'
No statement or runtime expression on this line
No statement or runtime expression on this line
Invalid line number value 'a'
Invalid line number value '4a'
No statement or runtime expression on this line
No statement or runtime expression on this line
Breakpoint already exists at line 4
Invalid breakpoint command
Invalid line number value 'foo'
No breakpoint on this line
)");
}

TEST_F(InteractiveDebuggerTest, SelectGroupAndInvocation) {
    Run(UVec3(2, 2, 2), {}, R"(
@compute @workgroup_size(2, 2, 2)
fn main() {
  var a = 0;
  var b = 0;
}
)",
        R"(
step
step
workgroup 1
wg 0 1
wg 0 0 1
wg 0 0 0
invocation 1
inv 0 1
inv 0 0 1
inv 0
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:5:3
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
   4:   var a = 0;
-> 5:   var b = 0;
        ^^^^^
   6: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:1
   3: fn main() {
   4:   var a = 0;
   5:   var b = 0;
-> 6: }
      ^
* workgroup_id(1,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,1,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,0,1)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:1
   3: fn main() {
   4:   var a = 0;
   5:   var b = 0;
-> 6: }
      ^
* workgroup_id(0,0,0)
  * local_invocation_id(1,0,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,1,0)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,1)
    * frame #0: main() at test.wgsl:4:3
   1:
   2: @compute @workgroup_size(2, 2, 2)
   3: fn main() {
-> 4:   var a = 0;
        ^^^^^
   5:   var b = 0;
   6: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:6:1
   3: fn main() {
   4:   var a = 0;
   5:   var b = 0;
-> 6: }
      ^
)",
        R"()");
}

TEST_F(InteractiveDebuggerTest, SelectGroupAndInvocation_Invalid) {
    Run(UVec3(2, 3, 4), {}, R"(
@compute @workgroup_size(5, 6, 7)
fn main() {}
)",
        R"(
wg
wg a
wg 1a
wg 0 0 3a
wg 0 0 4
wg 0 3
wg 2
inv
inv a
inv 1a
inv 0 0 3a
inv 0 0 7
inv 0 6
inv 5
step
inv 0
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:3:12
   1:
   2: @compute @workgroup_size(5, 6, 7)
-> 3: fn main() {}
                 ^
* workgroup_id(0,0,0)
  * local_invocation_id(1,0,0)
    * frame #0: main() at test.wgsl:3:12
   1:
   2: @compute @workgroup_size(5, 6, 7)
-> 3: fn main() {}
                 ^
)",
        R"(Expected 'workgroup group_id_x [group_id_y [group_id_z]]'
Invalid group_id.x value 'a'
Invalid group_id.x value '1a'
Invalid group_id.z value '3a'
workgroup_id(0,0,4) is not in the dispatch.
Total workgroup count: (2,3,4)
workgroup_id(0,3,0) is not in the dispatch.
Total workgroup count: (2,3,4)
workgroup_id(2,0,0) is not in the dispatch.
Total workgroup count: (2,3,4)
Expected 'invocation local_id_x [local_id_y [local_id_z]]'
Invalid local_id.x value 'a'
Invalid local_id.x value '1a'
Invalid local_id.z value '3a'
local_invocation_id(0,0,7) is not valid.
Workgroup size: (5,6,7)
local_invocation_id(0,6,0) is not valid.
Workgroup size: (5,6,7)
local_invocation_id(5,0,0) is not valid.
Workgroup size: (5,6,7)
local_invocation_id(0,0,0) has finished or is waiting at a barrier.
)");
}

TEST_F(InteractiveDebuggerTest, MultilineHighlights) {
    Run(UVec3(1, 1, 1), {}, R"(
fn foo(x : i32) -> i32 {
  return x + 1;    // break 3
}

@compute @workgroup_size(1)
fn main() {
  var
        a : i32;
        var b :
i32;
  var                 
c
:
i32;
  { var d : i32; var e : i32; }
}
)",
        R"(
step
step
step
step
step
step
continue
)",
        R"(* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:8:3
    5:
    6: @compute @workgroup_size(1)
    7: fn main() {
->  8:   var
         ^^^
    9:         a : i32;
   10:         var b :
   11: i32;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:10:9
    7: fn main() {
    8:   var
    9:         a : i32;
-> 10:         var b :
               ^^^^^^^
   11: i32;
   12:   var                 
   13: c
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:12:3
    9:         a : i32;
   10:         var b :
   11: i32;
-> 12:   var                 
         ^^^^^^^^^^^^^^^^^^^^
   13: c
   14: :
   15: i32;
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:16:3
   13: c
   14: :
   15: i32;
-> 16:   { var d : i32; var e : i32; }
         ^
   17: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:16:5
   13: c
   14: :
   15: i32;
-> 16:   { var d : i32; var e : i32; }
           ^^^^^^^^^^^
   17: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:16:18
   13: c
   14: :
   15: i32;
-> 16:   { var d : i32; var e : i32; }
                        ^^^^^^^^^^^
   17: }
* workgroup_id(0,0,0)
  * local_invocation_id(0,0,0)
    * frame #0: main() at test.wgsl:16:31
   13: c
   14: :
   15: i32;
-> 16:   { var d : i32; var e : i32; }
                                     ^
   17: }
)",
        R"()");
}

}  // namespace
}  // namespace tint::interp
