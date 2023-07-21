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

#include "src/tint/ir/transform/rename_conflicts_wgsl.h"

#include <utility>

#include "src/tint/ir/transform/test_helper.h"
#include "src/tint/lang/core/type/matrix.h"

namespace tint::ir::transform {
namespace {

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

using IR_RenameConflictsWGSLTest = TransformTest;

TEST_F(IR_RenameConflictsWGSLTest, NoModify_SingleNamedRootBlockVar) {
    b.With(b.RootBlock(), [&] { b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "v"); });

    auto* src = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_TwoRootBlockVarsWithSameName) {
    b.With(b.RootBlock(), [&] {
        b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "v");
        b.ir.SetName(b.Var(ty.ptr<private_, u32>()), "v");
    });

    auto* src = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
  %v_1:ptr<private, u32, read_write> = var  # %v_1: 'v'
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
  %v_1:ptr<private, u32, read_write> = var
}

)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_RootBlockVarAndStructWithSameName) {
    auto* s = ty.Struct(b.ir.symbols.New("v"), {{b.ir.symbols.New("x"), ty.i32()}});
    b.With(b.RootBlock(), [&] { b.ir.SetName(b.Var(ty.ptr(function, s)), "v"); });

    auto* src = R"(
v = struct @align(4) {
  x:i32 @offset(0)
}

%b1 = block {  # root
  %v:ptr<function, v, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
v = struct @align(4) {
  x:i32 @offset(0)
}

%b1 = block {  # root
  %v_1:ptr<function, v, read_write> = var
}

)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_RootBlockVarAndFnWithSameName) {
    b.With(b.RootBlock(), [&] { b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "v"); });

    auto* fn = b.Function("v", ty.i32());
    b.With(fn->Block(), [&] { b.Return(fn); });

    auto* src = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
}

%v_1 = func():i32 -> %b2 {  # %v_1: 'v'
  %b2 = block {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
}

%v_1 = func():i32 -> %b2 {
  %b2 = block {
    ret
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_RootBlockVar_ShadowedBy_FnVar) {
    b.With(b.RootBlock(), [&] {
        auto* outer = b.Var(ty.ptr<private_, i32>());
        b.ir.SetName(outer, "v");

        auto* fn = b.Function("f", ty.i32());
        b.With(fn->Block(), [&] {
            auto* load_outer = b.Load(outer);

            auto* inner = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(inner, "v");

            auto* load_inner = b.Load(inner);
            b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
        });
    });

    auto* src = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
}

%f = func():i32 -> %b2 {
  %b2 = block {
    %3:i32 = load %v
    %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
    %5:f32 = load %v_1
    %6:i32 = add %3, %5
    ret %6
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_RootBlockVar_ShadowedBy_FnVar) {
    b.With(b.RootBlock(), [&] {
        auto* outer = b.Var(ty.ptr<private_, i32>());
        b.ir.SetName(outer, "v");

        auto* fn = b.Function("f", ty.i32());
        b.With(fn->Block(), [&] {
            auto* inner = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(inner, "v");

            auto* load_outer = b.Load(outer);
            auto* load_inner = b.Load(inner);
            b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
        });
    });

    auto* src = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
}

%f = func():i32 -> %b2 {
  %b2 = block {
    %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
    %4:i32 = load %v
    %5:f32 = load %v_1
    %6:i32 = add %4, %5
    ret %6
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %v:ptr<private, i32, read_write> = var
}

%f = func():i32 -> %b2 {
  %b2 = block {
    %v_1:ptr<function, f32, read_write> = var
    %4:i32 = load %v
    %5:f32 = load %v_1
    %6:i32 = add %4, %5
    ret %6
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_FnVar_ShadowedBy_IfVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* outer = b.Var(ty.ptr<function, f32>());
        b.ir.SetName(outer, "v");

        auto* if_ = b.If(true);
        b.With(if_->True(), [&] {
            auto* load_outer = b.Load(outer);

            auto* inner = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(inner, "v");

            auto* load_inner = b.Load(inner);
            b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
        });

        b.Unreachable();
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %v:ptr<function, f32, read_write> = var
    if true [t: %b2] {  # if_1
      %b2 = block {  # true
        %3:f32 = load %v
        %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
        %5:f32 = load %v_1
        %6:i32 = add %3, %5
        ret %6
      }
    }
    unreachable
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_FnVar_ShadowedBy_IfVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* outer = b.Var(ty.ptr<function, f32>());
        b.ir.SetName(outer, "v");

        auto* if_ = b.If(true);
        b.With(if_->True(), [&] {
            auto* inner = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(inner, "v");

            auto* load_outer = b.Load(outer);
            auto* load_inner = b.Load(inner);
            b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
        });

        b.Unreachable();
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %v:ptr<function, f32, read_write> = var
    if true [t: %b2] {  # if_1
      %b2 = block {  # true
        %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
        %4:f32 = load %v
        %5:f32 = load %v_1
        %6:i32 = add %4, %5
        ret %6
      }
    }
    unreachable
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %v:ptr<function, f32, read_write> = var
    if true [t: %b2] {  # if_1
      %b2 = block {  # true
        %v_1:ptr<function, f32, read_write> = var
        %4:f32 = load %v
        %5:f32 = load %v_1
        %6:i32 = add %4, %5
        ret %6
      }
    }
    unreachable
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_LoopInitVar_ShadowedBy_LoopBodyVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* loop = b.Loop();
        b.With(loop->Initializer(), [&] {
            auto* outer = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(outer, "v");
            b.NextIteration(loop);

            b.With(loop->Body(), [&] {
                auto* load_outer = b.Load(outer);

                auto* inner = b.Var(ty.ptr<function, f32>());
                b.ir.SetName(inner, "v");

                auto* load_inner = b.Load(inner);
                b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
            });
        });

        b.Unreachable();
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    loop [i: %b2, b: %b3] {  # loop_1
      %b2 = block {  # initializer
        %v:ptr<function, f32, read_write> = var
        next_iteration %b3
      }
      %b3 = block {  # body
        %3:f32 = load %v
        %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
        %5:f32 = load %v_1
        %6:i32 = add %3, %5
        ret %6
      }
    }
    unreachable
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_LoopInitVar_ShadowedBy_LoopBodyVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* loop = b.Loop();
        b.With(loop->Initializer(), [&] {
            auto* outer = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(outer, "v");
            b.NextIteration(loop);

            b.With(loop->Body(), [&] {
                auto* inner = b.Var(ty.ptr<function, f32>());
                b.ir.SetName(inner, "v");

                auto* load_outer = b.Load(outer);
                auto* load_inner = b.Load(inner);
                b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
            });
        });

        b.Unreachable();
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    loop [i: %b2, b: %b3] {  # loop_1
      %b2 = block {  # initializer
        %v:ptr<function, f32, read_write> = var
        next_iteration %b3
      }
      %b3 = block {  # body
        %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
        %4:f32 = load %v
        %5:f32 = load %v_1
        %6:i32 = add %4, %5
        ret %6
      }
    }
    unreachable
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    loop [i: %b2, b: %b3] {  # loop_1
      %b2 = block {  # initializer
        %v:ptr<function, f32, read_write> = var
        next_iteration %b3
      }
      %b3 = block {  # body
        %v_1:ptr<function, f32, read_write> = var
        %4:f32 = load %v
        %5:f32 = load %v_1
        %6:i32 = add %4, %5
        ret %6
      }
    }
    unreachable
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_LoopBodyVar_ShadowedBy_LoopContVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* loop = b.Loop();
        b.With(loop->Initializer(), [&] { b.NextIteration(loop); });
        b.With(loop->Body(), [&] {
            auto* outer = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(outer, "v");
            b.Continue(loop);

            b.With(loop->Continuing(), [&] {
                auto* load_outer = b.Load(outer);

                auto* inner = b.Var(ty.ptr<function, f32>());
                b.ir.SetName(inner, "v");

                auto* load_inner = b.Load(inner);
                b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
            });
        });

        b.Unreachable();
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    loop [i: %b2, b: %b3, c: %b4] {  # loop_1
      %b2 = block {  # initializer
        next_iteration %b3
      }
      %b3 = block {  # body
        %v:ptr<function, f32, read_write> = var
        continue %b4
      }
      %b4 = block {  # continuing
        %3:f32 = load %v
        %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
        %5:f32 = load %v_1
        %6:i32 = add %3, %5
        ret %6
      }
    }
    unreachable
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_LoopBodyVar_ShadowedBy_LoopContVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* loop = b.Loop();
        b.With(loop->Initializer(), [&] { b.NextIteration(loop); });
        b.With(loop->Body(), [&] {
            auto* outer = b.Var(ty.ptr<function, f32>());
            b.ir.SetName(outer, "v");
            b.Continue(loop);

            b.With(loop->Continuing(), [&] {
                auto* inner = b.Var(ty.ptr<function, f32>());
                b.ir.SetName(inner, "v");

                auto* load_outer = b.Load(outer);
                auto* load_inner = b.Load(inner);
                b.Return(fn, b.Add(ty.i32(), load_outer, load_inner));
            });
        });

        b.Unreachable();
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    loop [i: %b2, b: %b3, c: %b4] {  # loop_1
      %b2 = block {  # initializer
        next_iteration %b3
      }
      %b3 = block {  # body
        %v:ptr<function, f32, read_write> = var
        continue %b4
      }
      %b4 = block {  # continuing
        %v_1:ptr<function, f32, read_write> = var  # %v_1: 'v'
        %4:f32 = load %v
        %5:f32 = load %v_1
        %6:i32 = add %4, %5
        ret %6
      }
    }
    unreachable
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    loop [i: %b2, b: %b3, c: %b4] {  # loop_1
      %b2 = block {  # initializer
        next_iteration %b3
      }
      %b3 = block {  # body
        %v:ptr<function, f32, read_write> = var
        continue %b4
      }
      %b4 = block {  # continuing
        %v_1:ptr<function, f32, read_write> = var
        %4:f32 = load %v
        %5:f32 = load %v_1
        %6:i32 = add %4, %5
        ret %6
      }
    }
    unreachable
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinScalar_ShadowedBy_Param) {
    auto* fn = b.Function("f", ty.void_());
    auto* p = b.FunctionParam(ty.i32());
    b.ir.SetName(p, "i32");
    fn->SetParams({p});

    b.With(fn->Block(), [&] {
        b.Var(ty.ptr<function, i32>());
        b.Return(fn);
    });

    auto* src = R"(
%f = func(%i32:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, i32, read_write> = var
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func(%i32_1:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, i32, read_write> = var
    ret
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinVector_ShadowedBy_Param) {
    auto* fn = b.Function("f", ty.void_());
    auto* p = b.FunctionParam(ty.i32());
    b.ir.SetName(p, "vec2");
    fn->SetParams({p});

    b.With(fn->Block(), [&] {
        b.Var(ty.ptr<function, vec3<i32>>());
        b.Return(fn);
    });

    auto* src = R"(
%f = func(%vec2:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, vec3<i32>, read_write> = var
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinVector_ShadowedBy_Param) {
    auto* fn = b.Function("f", ty.void_());
    auto* p = b.FunctionParam(ty.i32());
    b.ir.SetName(p, "vec3");
    fn->SetParams({p});

    b.With(fn->Block(), [&] {
        b.Var(ty.ptr<function, vec3<i32>>());
        b.Return(fn);
    });

    auto* src = R"(
%f = func(%vec3:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, vec3<i32>, read_write> = var
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func(%vec3_1:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, vec3<i32>, read_write> = var
    ret
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinMatrix_ShadowedBy_Param) {
    auto* fn = b.Function("f", ty.void_());
    auto* p = b.FunctionParam(ty.i32());
    b.ir.SetName(p, "mat3x2");
    fn->SetParams({p});

    b.With(fn->Block(), [&] {
        b.Var(ty.ptr<function, mat2x4<f32>>());
        b.Return(fn);
    });

    auto* src = R"(
%f = func(%mat3x2:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, mat2x4<f32>, read_write> = var
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinMatrix_ShadowedBy_Param) {
    auto* fn = b.Function("f", ty.void_());
    auto* p = b.FunctionParam(ty.i32());
    b.ir.SetName(p, "mat2x4");
    fn->SetParams({p});

    b.With(fn->Block(), [&] {
        b.Var(ty.ptr<function, mat2x4<f32>>());
        b.Return(fn);
    });

    auto* src = R"(
%f = func(%mat2x4:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, mat2x4<f32>, read_write> = var
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func(%mat2x4_1:i32):void -> %b1 {
  %b1 = block {
    %3:ptr<function, mat2x4<f32>, read_write> = var
    ret
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinScalar_ShadowedBy_FnVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* v = b.Var(ty.ptr<function, i32>());
        b.ir.SetName(v, "f32");

        b.Return(fn, b.Construct(ty.i32()));
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %f32:ptr<function, i32, read_write> = var
    %3:i32 = construct
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinScalar_ShadowedBy_FnVar) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* v = b.Var(ty.ptr<function, i32>());
        b.ir.SetName(v, "i32");

        b.Return(fn, b.Construct(ty.i32()));
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %i32:ptr<function, i32, read_write> = var
    %3:i32 = construct
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %i32_1:ptr<function, i32, read_write> = var
    %3:i32 = construct
    ret %3
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinScalar_ShadowedBy_NamedInst) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* i = b.Add(ty.i32(), 1_i, 2_i);
        b.ir.SetName(i, "i32");

        b.Return(fn, i);
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %i32:i32 = add 1i, 2i
    ret %i32
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinScalar_ShadowedBy_NamedInst) {
    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {
        auto* i = b.Add(ty.i32(), 1_i, 2_i);
        b.ir.SetName(i, "f32");

        b.Return(fn, b.Construct(ty.f32(), i));
    });

    auto* src = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %f32:i32 = add 1i, 2i
    %3:f32 = construct %f32
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%f = func():i32 -> %b1 {
  %b1 = block {
    %f32_1:i32 = add 1i, 2i
    %3:f32 = construct %f32_1
    ret %3
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinAddressSpace_ShadowedBy_RootBlockVar) {
    b.With(b.RootBlock(), [&] {  //
        b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "function");
    });

    auto* src = R"(
%b1 = block {  # root
  %function:ptr<private, i32, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinAddressSpace_ShadowedBy_RootBlockVar) {
    b.With(b.RootBlock(), [&] {  //
        b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "private");
    });

    auto* src = R"(
%b1 = block {  # root
  %private:ptr<private, i32, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %private_1:ptr<private, i32, read_write> = var
}

)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinAccess_ShadowedBy_RootBlockVar) {
    b.With(b.RootBlock(), [&] {  //
        b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "read");
    });

    auto* src = R"(
%b1 = block {  # root
  %read:ptr<private, i32, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinAccess_ShadowedBy_RootBlockVar) {
    b.With(b.RootBlock(), [&] {  //
        b.ir.SetName(b.Var(ty.ptr<private_, i32>()), "read_write");
    });

    auto* src = R"(
%b1 = block {  # root
  %read_write:ptr<private, i32, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %read_write_1:ptr<private, i32, read_write> = var
}

)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, NoModify_BuiltinFn_ShadowedBy_RootBlockVar) {
    b.With(b.RootBlock(), [&] {  //
        auto* v = b.Var(ty.ptr<private_, i32>());
        b.ir.SetName(v, "min");
    });

    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {  //
        auto* res = b.Call(ty.i32(), builtin::Function::kMax, 1_i, 2_i)->Result();
        b.Return(fn, res);
    });

    auto* src = R"(
%b1 = block {  # root
  %min:ptr<private, i32, read_write> = var
}

%f = func():i32 -> %b2 {
  %b2 = block {
    %3:i32 = max 1i, 2i
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_RenameConflictsWGSLTest, Conflict_BuiltinFn_ShadowedBy_RootBlockVar) {
    b.With(b.RootBlock(), [&] {  //
        auto* v = b.Var(ty.ptr<private_, i32>());
        b.ir.SetName(v, "max");
    });

    auto* fn = b.Function("f", ty.i32());
    b.With(fn->Block(), [&] {  //
        auto* res = b.Call(ty.i32(), builtin::Function::kMax, 1_i, 2_i)->Result();
        b.Return(fn, res);
    });

    auto* src = R"(
%b1 = block {  # root
  %max:ptr<private, i32, read_write> = var
}

%f = func():i32 -> %b2 {
  %b2 = block {
    %3:i32 = max 1i, 2i
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %max_1:ptr<private, i32, read_write> = var
}

%f = func():i32 -> %b2 {
  %b2 = block {
    %3:i32 = max 1i, 2i
    ret %3
  }
}
)";

    Run<RenameConflictsWGSL>();

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::ir::transform
