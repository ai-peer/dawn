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

#include "src/tint/lang/core/ir/transform/zero_init_workgroup_memory.h"

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"

namespace tint::core::ir::transform {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

class IR_ZeroInitWorkgroupMemoryTest : public TransformTest {
  protected:
    Function* MakeEntryPoint(const char* name,
                             uint32_t wgsize_x,
                             uint32_t wgsize_y,
                             uint32_t wgsize_z) {
        auto* func = b.Function(name, ty.void_(), Function::PipelineStage::kCompute);
        func->SetWorkgroupSize(wgsize_x, wgsize_y, wgsize_z);
        return func;
    }

    Var* MakeVar(const char* name, const type::Type* store_type) {
        auto* var = b.Var(name, ty.ptr(workgroup, store_type));
        b.RootBlock()->Append(var);
        return var;
    }
};

TEST_F(IR_ZeroInitWorkgroupMemoryTest, NoRootBlock) {
    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    auto* expect = R"(
%main = @compute @workgroup_size(1, 1, 1) func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, WorkgroupVarUnused) {
    MakeVar("wgvar", ty.i32());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, i32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ScalarBool) {
    auto* var = MakeVar("wgvar", ty.bool_());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, bool, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:bool = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, bool, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, false
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:bool = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ScalarI32) {
    auto* var = MakeVar("wgvar", ty.i32());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, i32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:i32 = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, i32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, 0i
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:i32 = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ScalarU32) {
    auto* var = MakeVar("wgvar", ty.u32());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, u32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:u32 = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, u32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, 0u
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:u32 = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ScalarF32) {
    auto* var = MakeVar("wgvar", ty.f32());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, f32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:f32 = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, f32, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, 0.0f
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:f32 = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ScalarF16) {
    auto* var = MakeVar("wgvar", ty.f16());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, f16, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:f16 = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, f16, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, 0.0h
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:f16 = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, AtomicI32) {
    auto* var = MakeVar("wgvar", ty.atomic<i32>());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Call(ty.i32(), core::Function::kAtomicLoad, var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, atomic<i32>, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:i32 = atomicLoad %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, atomic<i32>, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        %5:void = atomicStore %wgvar, 0i
        exit_if  # if_1
      }
    }
    %6:void = workgroupBarrier
    %7:i32 = atomicLoad %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, AtomicU32) {
    auto* var = MakeVar("wgvar", ty.atomic<u32>());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Call(ty.u32(), core::Function::kAtomicLoad, var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, atomic<u32>, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:u32 = atomicLoad %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, atomic<u32>, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        %5:void = atomicStore %wgvar, 0u
        exit_if  # if_1
      }
    }
    %6:void = workgroupBarrier
    %7:u32 = atomicLoad %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ArrayOfI32) {
    auto* var = MakeVar("wgvar", ty.array<i32, 4>());

    auto* func = MakeEntryPoint("main", 11, 2, 3);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, array<i32, 4>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func():void -> %b2 {
  %b2 = block {
    %3:array<i32, 4> = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, array<i32, 4>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    loop [i: %b3, b: %b4, c: %b5] {  # loop_1
      %b3 = block {  # initializer
        next_iteration %b4 %3
      }
      %b4 = block (%4:u32) {  # body
        %5:bool = gt %4:u32, 4u
        if %5 [t: %b6] {  # if_1
          %b6 = block {  # true
            exit_loop  # loop_1
          }
        }
        %6:u32 = mod %4:u32, 4u
        %7:ptr<workgroup, i32, read_write> = access %wgvar, %6
        store %7, 0i
        continue %b5
      }
      %b5 = block {  # continuing
        %8:u32 = add %4:u32, 66u
        next_iteration %b4 %8
      }
    }
    %9:void = workgroupBarrier
    %10:array<i32, 4> = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ArrayOfArrayOfU32) {
    auto* var = MakeVar("wgvar", ty.array(ty.array<u32, 5>(), 7));

    auto* func = MakeEntryPoint("main", 11, 2, 3);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, array<array<u32, 5>, 7>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func():void -> %b2 {
  %b2 = block {
    %3:array<array<u32, 5>, 7> = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, array<array<u32, 5>, 7>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    loop [i: %b3, b: %b4, c: %b5] {  # loop_1
      %b3 = block {  # initializer
        next_iteration %b4 %3
      }
      %b4 = block (%4:u32) {  # body
        %5:bool = gt %4:u32, 35u
        if %5 [t: %b6] {  # if_1
          %b6 = block {  # true
            exit_loop  # loop_1
          }
        }
        %6:u32 = mod %4:u32, 7u
        %7:u32 = div %4:u32, 7u
        %8:u32 = mod %7, 5u
        %9:ptr<workgroup, u32, read_write> = access %wgvar, %6, %8
        store %9, 0u
        continue %b5
      }
      %b5 = block {  # continuing
        %10:u32 = add %4:u32, 66u
        next_iteration %b4 %10
      }
    }
    %11:void = workgroupBarrier
    %12:array<array<u32, 5>, 7> = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, StructOfScalars) {
    auto* s = ty.Struct(mod.symbols.New("MyStruct"), {
                                                         {mod.symbols.New("a"), ty.i32()},
                                                         {mod.symbols.New("b"), ty.u32()},
                                                         {mod.symbols.New("c"), ty.f32()},
                                                     });
    auto* var = MakeVar("wgvar", s);

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
  c:f32 @offset(8)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, MyStruct, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:MyStruct = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
MyStruct = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
  c:f32 @offset(8)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, MyStruct, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, MyStruct(0i, 0u, 0.0f)
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:MyStruct = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, NestedStructOfScalars) {
    auto* inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("a"), ty.i32()},
                                                          {mod.symbols.New("b"), ty.u32()},
                                                      });
    auto* outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("c"), ty.f32()},
                                                          {mod.symbols.New("inner"), inner},
                                                          {mod.symbols.New("d"), ty.bool_()},
                                                      });
    auto* var = MakeVar("wgvar", outer);

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
Inner = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
}

Outer = struct @align(4) {
  c:f32 @offset(0)
  inner:Inner @offset(4)
  d:bool @offset(12)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, Outer, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:Outer = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inner = struct @align(4) {
  a:i32 @offset(0)
  b:u32 @offset(4)
}

Outer = struct @align(4) {
  c:f32 @offset(0)
  inner:Inner @offset(4)
  d:bool @offset(12)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, Outer, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, Outer(0.0f, Inner(0i, 0u), false)
        exit_if  # if_1
      }
    }
    %5:void = workgroupBarrier
    %6:Outer = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, NestedStructOfScalarsWithAtomic) {
    auto* inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("a"), ty.i32()},
                                                          {mod.symbols.New("b"), ty.atomic<u32>()},
                                                      });
    auto* outer = ty.Struct(mod.symbols.New("Outer"), {
                                                          {mod.symbols.New("c"), ty.f32()},
                                                          {mod.symbols.New("inner"), inner},
                                                          {mod.symbols.New("d"), ty.bool_()},
                                                      });
    auto* var = MakeVar("wgvar", outer);

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
Inner = struct @align(4) {
  a:i32 @offset(0)
  b:atomic<u32> @offset(4)
}

Outer = struct @align(4) {
  c:f32 @offset(0)
  inner:Inner @offset(4)
  d:bool @offset(12)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, Outer, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func():void -> %b2 {
  %b2 = block {
    %3:Outer = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inner = struct @align(4) {
  a:i32 @offset(0)
  b:atomic<u32> @offset(4)
}

Outer = struct @align(4) {
  c:f32 @offset(0)
  inner:Inner @offset(4)
  d:bool @offset(12)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, Outer, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %4:bool = eq %3, 0u
    if %4 [t: %b3] {  # if_1
      %b3 = block {  # true
        %5:ptr<workgroup, f32, read_write> = access %wgvar, 0u
        store %5, 0.0f
        %6:ptr<workgroup, i32, read_write> = access %wgvar, 1u, 0u
        store %6, 0i
        %7:ptr<workgroup, atomic<u32>, read_write> = access %wgvar, 1u, 1u
        %8:void = atomicStore %7, 0u
        %9:ptr<workgroup, bool, read_write> = access %wgvar, 2u
        store %9, false
        exit_if  # if_1
      }
    }
    %10:void = workgroupBarrier
    %11:Outer = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ArrayOfStructOfArrayOfStructWithAtomic) {
    auto* inner = ty.Struct(mod.symbols.New("Inner"), {
                                                          {mod.symbols.New("a"), ty.i32()},
                                                          {mod.symbols.New("b"), ty.atomic<u32>()},
                                                      });
    auto* outer =
        ty.Struct(mod.symbols.New("Outer"), {
                                                {mod.symbols.New("c"), ty.f32()},
                                                {mod.symbols.New("inner"), ty.array(inner, 13)},
                                                {mod.symbols.New("d"), ty.bool_()},
                                            });
    auto* var = MakeVar("wgvar", ty.array(outer, 7));

    auto* func = MakeEntryPoint("main", 7, 3, 2);
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
Inner = struct @align(4) {
  a:i32 @offset(0)
  b:atomic<u32> @offset(4)
}

Outer = struct @align(4) {
  c:f32 @offset(0)
  inner:array<Inner, 13> @offset(4)
  d:bool @offset(108)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, array<Outer, 7>, read_write> = var
}

%main = @compute @workgroup_size(7, 3, 2) func():void -> %b2 {
  %b2 = block {
    %3:array<Outer, 7> = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inner = struct @align(4) {
  a:i32 @offset(0)
  b:atomic<u32> @offset(4)
}

Outer = struct @align(4) {
  c:f32 @offset(0)
  inner:array<Inner, 13> @offset(4)
  d:bool @offset(108)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, array<Outer, 7>, read_write> = var
}

%main = @compute @workgroup_size(7, 3, 2) func(%3:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    loop [i: %b3, b: %b4, c: %b5] {  # loop_1
      %b3 = block {  # initializer
        next_iteration %b4 %3
      }
      %b4 = block (%4:u32) {  # body
        %5:bool = gt %4:u32, 7u
        if %5 [t: %b6] {  # if_1
          %b6 = block {  # true
            exit_loop  # loop_1
          }
        }
        %6:u32 = mod %4:u32, 7u
        %7:ptr<workgroup, f32, read_write> = access %wgvar, %6, 0u
        store %7, 0.0f
        %8:u32 = mod %4:u32, 7u
        %9:ptr<workgroup, bool, read_write> = access %wgvar, %8, 2u
        store %9, false
        continue %b5
      }
      %b5 = block {  # continuing
        %10:u32 = add %4:u32, 42u
        next_iteration %b4 %10
      }
    }
    loop [i: %b7, b: %b8, c: %b9] {  # loop_2
      %b7 = block {  # initializer
        next_iteration %b8 %3
      }
      %b8 = block (%11:u32) {  # body
        %12:bool = gt %11:u32, 91u
        if %12 [t: %b10] {  # if_2
          %b10 = block {  # true
            exit_loop  # loop_2
          }
        }
        %13:u32 = mod %11:u32, 7u
        %14:u32 = div %11:u32, 7u
        %15:u32 = mod %14, 13u
        %16:ptr<workgroup, i32, read_write> = access %wgvar, %13, 1u, %15, 0u
        store %16, 0i
        %17:u32 = mod %11:u32, 7u
        %18:u32 = div %11:u32, 7u
        %19:u32 = mod %18, 13u
        %20:ptr<workgroup, atomic<u32>, read_write> = access %wgvar, %17, 1u, %19, 1u
        %21:void = atomicStore %20, 0u
        continue %b9
      }
      %b9 = block {  # continuing
        %22:u32 = add %11:u32, 42u
        next_iteration %b8 %22
      }
    }
    %23:void = workgroupBarrier
    %24:array<Outer, 7> = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, MultipleVariables_DifferentIterationCounts) {
    auto* var_a = MakeVar("var_a", ty.bool_());
    auto* var_b = MakeVar("var_b", ty.array<i32, 4>());
    auto* var_c = MakeVar("var_c", ty.array(ty.array<u32, 5>(), 7));

    auto* func = MakeEntryPoint("main", 11, 2, 3);
    b.Append(func->Block(), [&] {  //
        b.Load(var_a);
        b.Load(var_b);
        b.Load(var_c);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %var_a:ptr<workgroup, bool, read_write> = var
  %var_b:ptr<workgroup, array<i32, 4>, read_write> = var
  %var_c:ptr<workgroup, array<array<u32, 5>, 7>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func():void -> %b2 {
  %b2 = block {
    %5:bool = load %var_a
    %6:array<i32, 4> = load %var_b
    %7:array<array<u32, 5>, 7> = load %var_c
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %var_a:ptr<workgroup, bool, read_write> = var
  %var_b:ptr<workgroup, array<i32, 4>, read_write> = var
  %var_c:ptr<workgroup, array<array<u32, 5>, 7>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func(%5:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %6:bool = eq %5, 0u
    if %6 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %var_a, false
        exit_if  # if_1
      }
    }
    loop [i: %b4, b: %b5, c: %b6] {  # loop_1
      %b4 = block {  # initializer
        next_iteration %b5 %5
      }
      %b5 = block (%7:u32) {  # body
        %8:bool = gt %7:u32, 4u
        if %8 [t: %b7] {  # if_2
          %b7 = block {  # true
            exit_loop  # loop_1
          }
        }
        %9:u32 = mod %7:u32, 4u
        %10:ptr<workgroup, i32, read_write> = access %var_b, %9
        store %10, 0i
        continue %b6
      }
      %b6 = block {  # continuing
        %11:u32 = add %7:u32, 66u
        next_iteration %b5 %11
      }
    }
    loop [i: %b8, b: %b9, c: %b10] {  # loop_2
      %b8 = block {  # initializer
        next_iteration %b9 %5
      }
      %b9 = block (%12:u32) {  # body
        %13:bool = gt %12:u32, 35u
        if %13 [t: %b11] {  # if_3
          %b11 = block {  # true
            exit_loop  # loop_2
          }
        }
        %14:u32 = mod %12:u32, 7u
        %15:u32 = div %12:u32, 7u
        %16:u32 = mod %15, 5u
        %17:ptr<workgroup, u32, read_write> = access %var_c, %14, %16
        store %17, 0u
        continue %b10
      }
      %b10 = block {  # continuing
        %18:u32 = add %12:u32, 66u
        next_iteration %b9 %18
      }
    }
    %19:void = workgroupBarrier
    %20:bool = load %var_a
    %21:array<i32, 4> = load %var_b
    %22:array<array<u32, 5>, 7> = load %var_c
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, MultipleVariables_SharedIterationCounts) {
    auto* var_a = MakeVar("var_a", ty.bool_());
    auto* var_b = MakeVar("var_b", ty.i32());
    auto* var_c = MakeVar("var_c", ty.array<i32, 42>());
    auto* var_d = MakeVar("var_d", ty.array(ty.array<u32, 6>(), 7));

    auto* func = MakeEntryPoint("main", 11, 2, 3);
    b.Append(func->Block(), [&] {  //
        b.Load(var_a);
        b.Load(var_b);
        b.Load(var_c);
        b.Load(var_d);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %var_a:ptr<workgroup, bool, read_write> = var
  %var_b:ptr<workgroup, i32, read_write> = var
  %var_c:ptr<workgroup, array<i32, 42>, read_write> = var
  %var_d:ptr<workgroup, array<array<u32, 6>, 7>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func():void -> %b2 {
  %b2 = block {
    %6:bool = load %var_a
    %7:i32 = load %var_b
    %8:array<i32, 42> = load %var_c
    %9:array<array<u32, 6>, 7> = load %var_d
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %var_a:ptr<workgroup, bool, read_write> = var
  %var_b:ptr<workgroup, i32, read_write> = var
  %var_c:ptr<workgroup, array<i32, 42>, read_write> = var
  %var_d:ptr<workgroup, array<array<u32, 6>, 7>, read_write> = var
}

%main = @compute @workgroup_size(11, 2, 3) func(%6:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %7:bool = eq %6, 0u
    if %7 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %var_a, false
        store %var_b, 0i
        exit_if  # if_1
      }
    }
    loop [i: %b4, b: %b5, c: %b6] {  # loop_1
      %b4 = block {  # initializer
        next_iteration %b5 %6
      }
      %b5 = block (%8:u32) {  # body
        %9:bool = gt %8:u32, 42u
        if %9 [t: %b7] {  # if_2
          %b7 = block {  # true
            exit_loop  # loop_1
          }
        }
        %10:u32 = mod %8:u32, 42u
        %11:ptr<workgroup, i32, read_write> = access %var_c, %10
        store %11, 0i
        %12:u32 = mod %8:u32, 7u
        %13:u32 = div %8:u32, 7u
        %14:u32 = mod %13, 6u
        %15:ptr<workgroup, u32, read_write> = access %var_d, %12, %14
        store %15, 0u
        continue %b6
      }
      %b6 = block {  # continuing
        %16:u32 = add %8:u32, 66u
        next_iteration %b5 %16
      }
    }
    %17:void = workgroupBarrier
    %18:bool = load %var_a
    %19:i32 = load %var_b
    %20:array<i32, 42> = load %var_c
    %21:array<array<u32, 6>, 7> = load %var_d
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ExistingLocalInvocationIndex) {
    auto* var = MakeVar("wgvar", ty.bool_());

    auto* func = MakeEntryPoint("main", 1, 1, 1);
    auto* global_id = b.FunctionParam("global_id", ty.vec4<u32>());
    global_id->SetBuiltin(FunctionParam::Builtin::kGlobalInvocationId);
    auto* index = b.FunctionParam("index", ty.u32());
    index->SetBuiltin(FunctionParam::Builtin::kLocalInvocationIndex);
    func->SetParams({global_id, index});
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, bool, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%global_id:vec4<u32> [@global_invocation_id], %index:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %5:bool = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %wgvar:ptr<workgroup, bool, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%global_id:vec4<u32> [@global_invocation_id], %index:u32 [@local_invocation_index]):void -> %b2 {
  %b2 = block {
    %5:bool = eq %index, 0u
    if %5 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, false
        exit_if  # if_1
      }
    }
    %6:void = workgroupBarrier
    %7:bool = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ZeroInitWorkgroupMemoryTest, ExistingLocalInvocationIndexInStruct) {
    auto* var = MakeVar("wgvar", ty.bool_());

    auto* structure =
        ty.Struct(mod.symbols.New("MyStruct"),
                  {
                      {
                          mod.symbols.New("global_id"),
                          ty.vec3<u32>(),
                          {{}, {}, core::BuiltinValue::kGlobalInvocationId, {}, false},
                      },
                      {
                          mod.symbols.New("index"),
                          ty.u32(),
                          {{}, {}, core::BuiltinValue::kLocalInvocationIndex, {}, false},
                      },
                  });
    auto* func = MakeEntryPoint("main", 1, 1, 1);
    func->SetParams({b.FunctionParam("params", structure)});
    b.Append(func->Block(), [&] {  //
        b.Load(var);
        b.Return(func);
    });

    auto* src = R"(
MyStruct = struct @align(16) {
  global_id:vec3<u32> @offset(0), @builtin(global_invocation_id)
  index:u32 @offset(12), @builtin(local_invocation_index)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, bool, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%params:MyStruct):void -> %b2 {
  %b2 = block {
    %4:bool = load %wgvar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
MyStruct = struct @align(16) {
  global_id:vec3<u32> @offset(0), @builtin(global_invocation_id)
  index:u32 @offset(12), @builtin(local_invocation_index)
}

%b1 = block {  # root
  %wgvar:ptr<workgroup, bool, read_write> = var
}

%main = @compute @workgroup_size(1, 1, 1) func(%params:MyStruct):void -> %b2 {
  %b2 = block {
    %4:u32 = access %params, 1u
    %5:bool = eq %4, 0u
    if %5 [t: %b3] {  # if_1
      %b3 = block {  # true
        store %wgvar, false
        exit_if  # if_1
      }
    }
    %6:void = workgroupBarrier
    %7:bool = load %wgvar
    ret
  }
}
)";

    Run(ZeroInitWorkgroupMemory);

    EXPECT_EQ(expect, str());
}

// TODO: uses nested inside blocks
// TODO: indirect uses via function

}  // namespace
}  // namespace tint::core::ir::transform
