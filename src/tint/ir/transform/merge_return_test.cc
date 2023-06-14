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

#include "src/tint/ir/transform/merge_return.h"

#include <utility>

#include "src/tint/ir/transform/test_helper.h"

namespace tint::ir::transform {
namespace {

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

using IR_MergeReturnTest = TransformTest;

TEST_F(IR_MergeReturnTest, NoModify_SingleReturnInRootBlock) {
    auto* in = b.FunctionParam(ty.i32());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({in});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());
    sb.Return(func, sb.Add(ty.i32(), in, 1_i));

    auto* src = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    %3:i32 = add %2, 1i
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<MergeReturn>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, NoModify_SingleReturnInMergeBlock) {
    auto* in = b.FunctionParam(ty.i32());
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({in});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    ifelse->SetType(ty.tuple<i32>());
    auto tb = b.With(ifelse->True());
    tb.ExitIf(ifelse, tb.Add(ty.i32(), in, 1_i));
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse, fb.Add(ty.i32(), in, 2_i));

    sb.Return(func, sb.Access(ty.i32(), ifelse, 0_i));

    auto* src = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    %3:[i32] = if %4 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        %5:i32 = add %2, 1i
        exit_if %5
      }

      # False block
      %b3 = block {
        %6:i32 = add %2, 2i
        exit_if %6
      }

    %7:i32 = access %3, 0i
    ret %7
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<MergeReturn>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, NoModify_SingleReturnInNestedMergeBlock) {
    auto* in = b.FunctionParam(ty.i32());
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({in});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* swtch = sb.Switch(in);
    b.Case(swtch, {Switch::CaseSelector{}})->Append(b.ExitSwitch(swtch));

    sb.Loop();

    auto* ifelse = sb.If(cond);
    ifelse->SetType(ty.tuple<i32>());
    auto tb = b.With(ifelse->True());
    tb.ExitIf(ifelse, tb.Add(ty.i32(), in, 1_i));
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse, fb.Add(ty.i32(), in, 2_i));

    sb.Return(func, sb.Access(ty.i32(), ifelse, 0_i));

    auto* src = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    switch %2 [c: (default, %b2)]
      # Case block
      %b2 = block {
        exit_switch
      }

    loop []
    %3:[i32] = if %4 [t: %b3, f: %b4]
      # True block
      %b3 = block {
        %5:i32 = add %2, 1i
        exit_if %5
      }

      # False block
      %b4 = block {
        %6:i32 = add %2, 2i
        exit_if %6
      }

    %7:i32 = access %3, 0i
    ret %7
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run<MergeReturn>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_OneSideReturns) {
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    auto tb = b.With(ifelse->True());
    tb.Return(func);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse);

    sb.Return(func);

    auto* src = R"(
%foo = func(%2:bool):void -> %b1 {
  %b1 = block {
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        ret
      }

      # False block
      %b3 = block {
        exit_if
      }

    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:bool):void -> %b1 {
  %b1 = block {
    %continue_execution:ptr<function, bool, read_write> = var, true
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        store %continue_execution, false
        exit_if
      }

      # False block
      %b3 = block {
        exit_if
      }

    ret
  }
}
)";

    Run<MergeReturn>();

    EXPECT_EQ(expect, str());
}

// This is the same as the above tests, but we create the return instructions in a different order
// to make sure that creation order doesn't matter.
TEST_F(IR_MergeReturnTest, IfElse_OneSideReturns_ReturnsCreatedInDifferentOrder) {
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    sb.Return(func);

    auto tb = b.With(ifelse->True());
    tb.Return(func);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse);

    auto* src = R"(
%foo = func(%2:bool):void -> %b1 {
  %b1 = block {
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        ret
      }

      # False block
      %b3 = block {
        exit_if
      }

    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:bool):void -> %b1 {
  %b1 = block {
    %continue_execution:ptr<function, bool, read_write> = var, true
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        store %continue_execution, false
        exit_if
      }

      # False block
      %b3 = block {
        exit_if
      }

    ret
  }
}
)";

    Run<MergeReturn>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_OneSideReturns_WithValue) {
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    auto tb = b.With(ifelse->True());
    tb.Return(func, 1_i);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse);

    sb.Return(func, 2_i);

    auto* src = R"(
%foo = func(%2:bool):i32 -> %b1 {
  %b1 = block {
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        ret 1i
      }

      # False block
      %b3 = block {
        exit_if
      }

    ret 2i
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:bool):i32 -> %b1 {
  %b1 = block {
    %return_value:ptr<function, i32, read_write> = var
    %continue_execution:ptr<function, bool, read_write> = var, true
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        store %continue_execution, false
        store %return_value, 1i
        exit_if
      }

      # False block
      %b3 = block {
        exit_if
      }

    %5:bool = load %continue_execution
    if %5 [t: %b4]
      # True block
      %b4 = block {
        store %return_value, 2i
        exit_if
      }

    %6:i32 = load %return_value
    ret %6
  }
}
)";

    Run<MergeReturn>();

    EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_OneSideReturns_WithValue_MergeHasBasicBlockArguments) {
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    ifelse->SetType(ty.tuple<i32>());
    auto tb = b.With(ifelse->True());
    tb.Return(func, 1_i);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse, 2_i);

    sb.Return(func, sb.Access(ty.i32(), ifelse, 0_i));

    auto* src = R"(
%foo = func(%2:bool):i32 -> %b1 {
  %b1 = block {
    %3:[i32] = if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        ret 1i
      }

      # False block
      %b3 = block {
        exit_if 2i
      }

    %4:i32 = access %3, 0i
    ret %4
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:bool):i32 -> %b1 {
  %b1 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    if %2 [t: %b2, f: %b3, m: %b4]
      # True block
      %b2 = block {
        store %return_flag, true
        store %return_value, 1i
        exit_if %b4 undef
      }

      # False block
      %b3 = block {
        exit_if %b4 2i
      }

    # Merge block
    %b4 = block (%5:i32) {
      %6:bool = load %return_flag
      if %6 [t: %b5, f: %b6, m: %b7]
        # True block
        %b5 = block {
          exit_if %b7
        }

        # False block
        %b6 = block {
          store %return_flag, true
          store %return_value, %5:i32
          exit_if %b7
        }

      # Merge block
      %b7 = block {
        %7:i32 = load %return_value
        ret %7
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_OneSideReturns_WithValue_MergeHasUndefBasicBlockArguments) {
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    ifelse->SetType(ty.tuple<i32>());
    auto tb = b.With(ifelse->True());
    tb.Return(func, 1_i);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse, b.Undef(ty.i32()));

    sb.Return(func, sb.Access(ty.i32(), ifelse, 0_i));

    auto* src = R"(
%foo = func(%2:bool):i32 -> %b1 {
  %b1 = block {
    %3:[i32] = if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        ret 1i
      }

      # False block
      %b3 = block {
        exit_if undef
      }

    %4:i32 = access %3, 0i
    ret %4
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:bool):i32 -> %b1 {
  %b1 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    if %2 [t: %b2, f: %b3, m: %b4]
      # True block
      %b2 = block {
        store %return_flag, true
        store %return_value, 1i
        exit_if %b4 undef
      }

      # False block
      %b3 = block {
        exit_if %b4 undef
      }

    # Merge block
    %b4 = block (%5:i32) {
      %6:bool = load %return_flag
      if %6 [t: %b5, f: %b6, m: %b7]
        # True block
        %b5 = block {
          exit_if %b7
        }

        # False block
        %b6 = block {
          store %return_flag, true
          store %return_value, %5:i32
          exit_if %b7
        }

      # Merge block
      %b7 = block {
        %7:i32 = load %return_value
        ret %7
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_BothSidesReturn) {
    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    auto tb = b.With(ifelse->True());
    tb.Return(func);
    auto fb = b.With(ifelse->False());
    fb.Return(func);

    auto* src = R"(
%foo = func(%2:bool):void -> %b1 {
  %b1 = block {
    if %2 [t: %b2, f: %b3]
      # True block
      %b2 = block {
        ret
      }

      # False block
      %b3 = block {
        ret
      }

  }
}
)";

    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:bool):void -> %b1 {
  %b1 = block {
    %return_flag:ptr<function, bool, read_write> = var, false
    if %2 [t: %b2, f: %b3, m: %b4]
      # True block
      %b2 = block {
        store %return_flag, true
        exit_if %b4
      }

      # False block
      %b3 = block {
        store %return_flag, true
        exit_if %b4
      }

    # Merge block
    %b4 = block {
      ret
    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_NonEmptyMergeBlock) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    auto tb = b.With(ifelse->True());
    tb.Return(func);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse);

    sb.Store(global, 42_i);
    sb.Return(func);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):void -> %b2 {
  %b2 = block {
    if %3 [t: %b3, f: %b4]
      # True block
      %b3 = block {
        ret
      }

      # False block
      %b4 = block {
        exit_if
      }

    store %1, 42i
    ret
  }
}
)";

    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):void -> %b2 {
  %b2 = block {
    %return_flag:ptr<function, bool, read_write> = var, false
    if %3 [t: %b3, f: %b4, m: %b5]
      # True block
      %b3 = block {
        store %return_flag, true
        exit_if %b5
      }

      # False block
      %b4 = block {
        exit_if %b5
      }

    # Merge block
    %b5 = block {
      %5:bool = load %return_flag
      if %5 [t: %b6, f: %b7, m: %b8]
        # True block
        %b6 = block {
          exit_if %b8
        }

        # False block
        %b7 = block {
          store %1, 42i
          store %return_flag, true
          exit_if %b8
        }

      # Merge block
      %b8 = block {
        ret
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

// This is the same as the above tests, but we create the return instructions in a different order
// to make sure that creation order doesn't matter.
TEST_F(IR_MergeReturnTest, IfElse_NonEmptyMergeBlock_ReturnsCreatedInDifferentOrder) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.void_());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse = sb.If(cond);
    sb.Store(global, 42_i);
    sb.Return(func);

    auto tb = b.With(ifelse->True());
    tb.Return(func);
    auto fb = b.With(ifelse->False());
    fb.ExitIf(ifelse);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):void -> %b2 {
  %b2 = block {
    if %3 [t: %b3, f: %b4]
      # True block
      %b3 = block {
        ret
      }

      # False block
      %b4 = block {
        exit_if
      }

    store %1, 42i
    ret
  }
}
)";

    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):void -> %b2 {
  %b2 = block {
    %return_flag:ptr<function, bool, read_write> = var, false
    if %3 [t: %b3, f: %b4, m: %b5]
      # True block
      %b3 = block {
        store %return_flag, true
        exit_if %b5
      }

      # False block
      %b4 = block {
        exit_if %b5
      }

    # Merge block
    %b5 = block {
      %5:bool = load %return_flag
      if %5 [t: %b6, f: %b7, m: %b8]
        # True block
        %b6 = block {
          exit_if %b8
        }

        # False block
        %b7 = block {
          store %1, 42i
          store %return_flag, true
          exit_if %b8
        }

      # Merge block
      %b8 = block {
        ret
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_Nested) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* func = b.Function("foo", ty.i32());
    auto* condA = b.FunctionParam(ty.bool_());
    auto* condB = b.FunctionParam(ty.bool_());
    auto* condC = b.FunctionParam(ty.bool_());
    mod.SetName(condA, "condA");
    mod.SetName(condB, "condB");
    mod.SetName(condC, "condC");
    func->SetParams({condA, condB, condC});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse_outer = sb.If(condA);
    auto outer_true = b.With(ifelse_outer->True());
    outer_true.Return(func, 3_i);
    auto outer_false = b.With(ifelse_outer->False());
    auto* ifelse_middle = outer_false.If(condB);

    sb.Store(global, 3_i);
    sb.Return(func, sb.Add(ty.i32(), 5_i, 6_i));

    auto middle_true = b.With(ifelse_middle->True());
    auto* ifelse_inner = middle_true.If(condC);
    auto middle_false = b.With(ifelse_middle->False());
    middle_false.ExitIf(ifelse_middle);

    outer_false.Store(global, 2_i);
    outer_false.ExitIf(ifelse_outer);

    auto inner_true = b.With(ifelse_inner->True());
    inner_true.Return(func, 1_i);
    auto inner_false = b.With(ifelse_inner->False());
    inner_false.ExitIf(ifelse_inner);

    middle_true.Store(global, 1_i);
    middle_true.Return(func, 2_i);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%condA:bool, %condB:bool, %condC:bool):i32 -> %b2 {
  %b2 = block {
    if %condA [t: %b3, f: %b4]
      # True block
      %b3 = block {
        ret 3i
      }

      # False block
      %b4 = block {
        if %condB [t: %b5, f: %b6]
          # True block
          %b5 = block {
            if %condC [t: %b7, f: %b8]
              # True block
              %b7 = block {
                ret 1i
              }

              # False block
              %b8 = block {
                exit_if
              }

            store %1, 1i
            ret 2i
          }

          # False block
          %b6 = block {
            exit_if
          }

        store %1, 2i
        exit_if
      }

    store %1, 3i
    %6:i32 = add 5i, 6i
    ret %6
  }
}
)";

    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%condA:bool, %condB:bool, %condC:bool):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    if %condA [t: %b3, f: %b4, m: %b5]
      # True block
      %b3 = block {
        store %return_flag, true
        store %return_value, 3i
        exit_if %b5
      }

      # False block
      %b4 = block {
        if %condB [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            if %condC [t: %b9, f: %b10, m: %b11]
              # True block
              %b9 = block {
                store %return_flag, true
                store %return_value, 1i
                exit_if %b11
              }

              # False block
              %b10 = block {
                exit_if %b11
              }

            # Merge block
            %b11 = block {
              %8:bool = load %return_flag
              if %8 [t: %b12, f: %b13, m: %b14]
                # True block
                %b12 = block {
                  exit_if %b14
                }

                # False block
                %b13 = block {
                  store %1, 1i
                  store %return_flag, true
                  store %return_value, 2i
                  exit_if %b14
                }

              # Merge block
              %b14 = block {
                exit_if %b8
              }

            }

          }

          # False block
          %b7 = block {
            exit_if %b8
          }

        # Merge block
        %b8 = block {
          %9:bool = load %return_flag
          if %9 [t: %b15, f: %b16, m: %b17]
            # True block
            %b15 = block {
              exit_if %b17
            }

            # False block
            %b16 = block {
              store %1, 2i
              exit_if %b17
            }

          # Merge block
          %b17 = block {
            exit_if %b5
          }

        }

      }

    # Merge block
    %b5 = block {
      %10:bool = load %return_flag
      if %10 [t: %b18, f: %b19, m: %b20]
        # True block
        %b18 = block {
          exit_if %b20
        }

        # False block
        %b19 = block {
          store %1, 3i
          %11:i32 = add 5i, 6i
          store %return_flag, true
          store %return_value, %11
          exit_if %b20
        }

      # Merge block
      %b20 = block {
        %12:i32 = load %return_value
        ret %12
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_Nested_TrivialMerge) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* func = b.Function("foo", ty.i32());
    auto* condA = b.FunctionParam(ty.bool_());
    auto* condB = b.FunctionParam(ty.bool_());
    auto* condC = b.FunctionParam(ty.bool_());
    mod.SetName(condA, "condA");
    mod.SetName(condB, "condB");
    mod.SetName(condC, "condC");
    func->SetParams({condA, condB, condC});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse_outer = sb.If(condA);
    auto outer_true = b.With(ifelse_outer->True());
    outer_true.Return(func, 3_i);
    auto outer_false = b.With(ifelse_outer->False());
    auto* ifelse_middle = outer_false.If(condB);

    sb.Return(func, 3_i);

    auto middle_true = b.With(ifelse_middle->True());
    auto* ifelse_inner = middle_true.If(condC);
    auto middle_false = b.With(ifelse_middle->False());
    middle_false.ExitIf(ifelse_middle);

    outer_false.ExitIf(ifelse_outer);

    auto inner_true = b.With(ifelse_inner->True());
    inner_true.Return(func, 1_i);
    auto inner_false = b.With(ifelse_inner->False());
    inner_false.ExitIf(ifelse_inner);

    middle_true.ExitIf(ifelse_middle);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%condA:bool, %condB:bool, %condC:bool):i32 -> %b2 {
  %b2 = block {
    if %condA [t: %b3, f: %b4]
      # True block
      %b3 = block {
        ret 3i
      }

      # False block
      %b4 = block {
        if %condB [t: %b5, f: %b6]
          # True block
          %b5 = block {
            if %condC [t: %b7, f: %b8]
              # True block
              %b7 = block {
                ret 1i
              }

              # False block
              %b8 = block {
                exit_if
              }

            exit_if
          }

          # False block
          %b6 = block {
            exit_if
          }

        exit_if
      }

    ret 3i
  }
}
)";

    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%condA:bool, %condB:bool, %condC:bool):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    if %condA [t: %b3, f: %b4, m: %b5]
      # True block
      %b3 = block {
        store %return_flag, true
        store %return_value, 3i
        exit_if %b5
      }

      # False block
      %b4 = block {
        if %condB [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            if %condC [t: %b9, f: %b10, m: %b11]
              # True block
              %b9 = block {
                store %return_flag, true
                store %return_value, 1i
                exit_if %b11
              }

              # False block
              %b10 = block {
                exit_if %b11
              }

            # Merge block
            %b11 = block {
              exit_if %b8
            }

          }

          # False block
          %b7 = block {
            exit_if %b8
          }

        # Merge block
        %b8 = block {
          exit_if %b5
        }

      }

    # Merge block
    %b5 = block {
      %8:bool = load %return_flag
      if %8 [t: %b12, f: %b13, m: %b14]
        # True block
        %b12 = block {
          exit_if %b14
        }

        # False block
        %b13 = block {
          store %return_flag, true
          store %return_value, 3i
          exit_if %b14
        }

      # Merge block
      %b14 = block {
        %9:i32 = load %return_value
        ret %9
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, IfElse_Nested_WithBasicBlockArguments) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* func = b.Function("foo", ty.i32());
    auto* condA = b.FunctionParam(ty.bool_());
    auto* condB = b.FunctionParam(ty.bool_());
    auto* condC = b.FunctionParam(ty.bool_());
    mod.SetName(condA, "condA");
    mod.SetName(condB, "condB");
    mod.SetName(condC, "condC");
    func->SetParams({condA, condB, condC});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* ifelse_outer = sb.If(condA);
    ifelse_outer->SetType(ty.tuple<i32>());
    auto outer_true = b.With(ifelse_outer->True());
    outer_true.Return(func, 3_i);
    auto outer_false = b.With(ifelse_outer->False());
    auto* ifelse_middle = outer_false.If(condB);
    ifelse_middle->SetType(ty.tuple<i32>());

    sb.Return(func, sb.Add(ty.i32(), sb.Access(ty.i32(), ifelse_outer, 0_i), 1_i));

    auto middle_true = b.With(ifelse_middle->True());
    auto* ifelse_inner = middle_true.If(condC);
    auto middle_false = b.With(ifelse_middle->False());
    middle_false.ExitIf(ifelse_middle, middle_false.Add(ty.i32(), 43_i, 2_i));

    outer_false.ExitIf(
        ifelse_outer,
        outer_false.Add(ty.i32(), outer_false.Access(ty.i32(), ifelse_middle, 0_i), 1_i));

    auto inner_true = b.With(ifelse_inner->True());
    inner_true.Return(func, 1_i);
    auto inner_false = b.With(ifelse_inner->False());
    inner_false.ExitIf(ifelse_inner);

    middle_true.ExitIf(ifelse_middle, middle_true.Add(ty.i32(), 42_i, 1_i));

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%condA:bool, %condB:bool, %condC:bool):i32 -> %b2 {
  %b2 = block {
    %6:[i32] = if %condA [t: %b3, f: %b4]
      # True block
      %b3 = block {
        ret 3i
      }

      # False block
      %b4 = block {
        %7:[i32] = if %condB [t: %b5, f: %b6]
          # True block
          %b5 = block {
            if %condC [t: %b7, f: %b8]
              # True block
              %b7 = block {
                ret 1i
              }

              # False block
              %b8 = block {
                exit_if
              }

            %8:i32 = add 42i, 1i
            exit_if %8
          }

          # False block
          %b6 = block {
            %9:i32 = add 43i, 2i
            exit_if %9
          }

        %10:i32 = access %7, 0i
        %11:i32 = add %10, 1i
        exit_if %11
      }

    %12:i32 = access %6, 0i
    %13:i32 = add %12, 1i
    ret %13
  }
}
)";

    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%condA:bool, %condB:bool, %condC:bool):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    if %condA [t: %b3, f: %b4, m: %b5]
      # True block
      %b3 = block {
        store %return_flag, true
        store %return_value, 3i
        exit_if %b5 undef
      }

      # False block
      %b4 = block {
        if %condB [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            if %condC [t: %b9, f: %b10, m: %b11]
              # True block
              %b9 = block {
                store %return_flag, true
                store %return_value, 1i
                exit_if %b11
              }

              # False block
              %b10 = block {
                exit_if %b11
              }

            # Merge block
            %b11 = block {
              %8:bool = load %return_flag
              if %8 [t: %b12, f: %b13, m: %b14]
                # True block
                %b12 = block {
                  exit_if %b14 undef
                }

                # False block
                %b13 = block {
                  %9:i32 = add 42i, 1i
                  exit_if %b14 %9
                }

              # Merge block
              %b14 = block (%10:i32) {
                exit_if %b8 %10:i32
              }

            }

          }

          # False block
          %b7 = block {
            %11:i32 = add 43i, 2i
            exit_if %b8 %11
          }

        # Merge block
        %b8 = block (%12:i32) {
          %13:bool = load %return_flag
          if %13 [t: %b15, f: %b16, m: %b17]
            # True block
            %b15 = block {
              exit_if %b17 undef
            }

            # False block
            %b16 = block {
              %14:i32 = add %12:i32, 1i
              exit_if %b17 %14
            }

          # Merge block
          %b17 = block (%15:i32) {
            exit_if %b5 %15:i32
          }

        }

      }

    # Merge block
    %b5 = block (%16:i32) {
      %17:bool = load %return_flag
      if %17 [t: %b18, f: %b19, m: %b20]
        # True block
        %b18 = block {
          exit_if %b20
        }

        # False block
        %b19 = block {
          %18:i32 = add %16:i32, 1i
          store %return_flag, true
          store %return_value, %18
          exit_if %b20
        }

      # Merge block
      %b20 = block {
        %19:i32 = load %return_value
        ret %19
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Loop_UnconditionalReturnInBody) {
    auto* func = b.Function("foo", ty.i32());
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* loop = sb.Loop();
    loop->Body()->Append(b.Return(func, 42_i));

    auto* src = R"(
%foo = func():i32 -> %b1 {
  %b1 = block {
    loop [b: %b2]
      # Body block
      %b2 = block {
        ret 42i
      }

  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func():i32 -> %b1 {
  %b1 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    loop [b: %b2, m: %b3]
      # Body block
      %b2 = block {
        store %return_flag, true
        store %return_value, 42i
        exit_loop %b3
      }

    # Merge block
    %b3 = block {
      %4:i32 = load %return_value
      ret %4
    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Loop_ConditionalReturnInBody) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* loop = sb.Loop();
    auto lb = b.With(loop->Body());
    auto* ifelse = lb.If(cond);
    {
        auto tb = b.With(ifelse->True());
        tb.Return(func, 42_i);
        auto fb = b.With(ifelse->False());
        fb.ExitIf(ifelse);
    }
    lb.Store(global, 2_i);
    lb.Continue(loop);

    auto cb = b.With(loop->Continuing());
    cb.Store(global, 1_i);
    cb.BreakIf(true, loop);

    sb.Store(global, 3_i);
    sb.Return(func, 43_i);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):i32 -> %b2 {
  %b2 = block {
    loop [b: %b3, c: %b4]
      # Body block
      %b3 = block {
        if %3 [t: %b5, f: %b6]
          # True block
          %b5 = block {
            ret 42i
          }

          # False block
          %b6 = block {
            exit_if
          }

        store %1, 2i
        continue %b4
      }

      # Continuing block
      %b4 = block {
        store %1, 1i
        break_if true %b3
      }

    store %1, 3i
    ret 43i
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    loop [b: %b3, c: %b4, m: %b5]
      # Body block
      %b3 = block {
        if %3 [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            store %return_flag, true
            store %return_value, 42i
            exit_if %b8
          }

          # False block
          %b7 = block {
            exit_if %b8
          }

        # Merge block
        %b8 = block {
          %6:bool = load %return_flag
          if %6 [t: %b9, f: %b10, m: %b11]
            # True block
            %b9 = block {
              exit_if %b11
            }

            # False block
            %b10 = block {
              store %1, 2i
              continue %b4
            }

          # Merge block
          %b11 = block {
            exit_loop %b5
          }

        }

      }

      # Continuing block
      %b4 = block {
        store %1, 1i
        break_if true %b3
      }

    # Merge block
    %b5 = block {
      %7:bool = load %return_flag
      if %7 [t: %b12, f: %b13, m: %b14]
        # True block
        %b12 = block {
          exit_if %b14
        }

        # False block
        %b13 = block {
          store %1, 3i
          store %return_flag, true
          store %return_value, 43i
          exit_if %b14
        }

      # Merge block
      %b14 = block {
        %8:i32 = load %return_value
        ret %8
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Loop_ConditionalReturnInBody_UnreachableMerge) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* loop = sb.Loop();
    auto lb = b.With(loop->Body());
    auto* ifelse = lb.If(cond);
    {
        auto tb = b.With(ifelse->True());
        tb.Return(func, 42_i);
        auto fb = b.With(ifelse->False());
        fb.ExitIf(ifelse);
    }
    lb.Store(global, 2_i);
    lb.Continue(loop);

    auto cb = b.With(loop->Continuing());
    cb.Store(global, 1_i);
    cb.NextIteration(loop);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):i32 -> %b2 {
  %b2 = block {
    loop [b: %b3, c: %b4]
      # Body block
      %b3 = block {
        if %3 [t: %b5, f: %b6]
          # True block
          %b5 = block {
            ret 42i
          }

          # False block
          %b6 = block {
            exit_if
          }

        store %1, 2i
        continue %b4
      }

      # Continuing block
      %b4 = block {
        store %1, 1i
        next_iteration %b3
      }

  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    loop [b: %b3, c: %b4, m: %b5]
      # Body block
      %b3 = block {
        if %3 [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            store %return_flag, true
            store %return_value, 42i
            exit_if %b8
          }

          # False block
          %b7 = block {
            exit_if %b8
          }

        # Merge block
        %b8 = block {
          %6:bool = load %return_flag
          if %6 [t: %b9, f: %b10, m: %b11]
            # True block
            %b9 = block {
              exit_if %b11
            }

            # False block
            %b10 = block {
              store %1, 2i
              continue %b4
            }

          # Merge block
          %b11 = block {
            exit_loop %b5
          }

        }

      }

      # Continuing block
      %b4 = block {
        store %1, 1i
        next_iteration %b3
      }

    # Merge block
    %b5 = block {
      %7:i32 = load %return_value
      ret %7
    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Loop_WithBasicBlockArgumentsOnMerge) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* cond = b.FunctionParam(ty.bool_());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* loop = sb.Loop();
    loop->SetType(ty.tuple<i32>());
    auto lb = b.With(loop->Body());
    auto* ifelse = lb.If(cond);
    {
        auto tb = b.With(ifelse->True());
        tb.Return(func, 42_i);
        auto fb = b.With(ifelse->False());
        fb.ExitIf(ifelse);
    }
    lb.Store(global, 2_i);
    lb.Continue(loop);

    auto cb = b.With(loop->Continuing());
    cb.Store(global, 1_i);
    cb.BreakIf(true, loop, 4_i);

    sb.Store(global, 3_i);
    sb.Return(func, sb.Access(ty.i32(), loop, 1_i));

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):i32 -> %b2 {
  %b2 = block {
    loop [b: %b3, c: %b4]
      # Body block
      %b3 = block {
        if %3 [t: %b5, f: %b6]
          # True block
          %b5 = block {
            ret 42i
          }

          # False block
          %b6 = block {
            exit_if
          }

        store %1, 2i
        continue %b4
      }

      # Continuing block
      %b4 = block {
        store %1, 1i
        break_if true %b3 4i
      }

    store %1, 3i
    %4:i32 = access %5, 1i
    ret %4
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:bool):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    loop [b: %b3, c: %b4, m: %b5]
      # Body block
      %b3 = block {
        if %3 [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            store %return_flag, true
            store %return_value, 42i
            exit_if %b8
          }

          # False block
          %b7 = block {
            exit_if %b8
          }

        # Merge block
        %b8 = block {
          %6:bool = load %return_flag
          if %6 [t: %b9, f: %b10, m: %b11]
            # True block
            %b9 = block {
              exit_if %b11
            }

            # False block
            %b10 = block {
              store %1, 2i
              continue %b4
            }

          # Merge block
          %b11 = block {
            exit_loop %b5 undef
          }

        }

      }

      # Continuing block
      %b4 = block {
        store %1, 1i
        break_if true %b3 4i
      }

    # Merge block
    %b5 = block (%7:i32) {
      %8:bool = load %return_flag
      if %8 [t: %b12, f: %b13, m: %b14]
        # True block
        %b12 = block {
          exit_if %b14
        }

        # False block
        %b13 = block {
          store %1, 3i
          store %return_flag, true
          store %return_value, %7:i32
          exit_if %b14
        }

      # Merge block
      %b14 = block {
        %9:i32 = load %return_value
        ret %9
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Switch_UnconditionalReturnInCase) {
    auto* cond = b.FunctionParam(ty.i32());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* sw = sb.Switch(cond);
    auto caseA = b.With(b.Case(sw, {Switch::CaseSelector{b.Constant(1_i)}}));
    caseA.Return(func, 42_i);
    auto caseB = b.With(b.Case(sw, {Switch::CaseSelector{}}));
    caseB.ExitSwitch(sw);

    sb.Return(func, 0_i);

    auto* src = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    switch %2 [c: (1i, %b2), c: (default, %b3)]
      # Case block
      %b2 = block {
        ret 42i
      }

      # Case block
      %b3 = block {
        exit_switch
      }

    ret 0i
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    switch %2 [c: (1i, %b2), c: (default, %b3), m: %b4]
      # Case block
      %b2 = block {
        store %return_flag, true
        store %return_value, 42i
        exit_switch %b4
      }

      # Case block
      %b3 = block {
        exit_switch %b4
      }

    # Merge block
    %b4 = block {
      %5:bool = load %return_flag
      if %5 [t: %b5, f: %b6, m: %b7]
        # True block
        %b5 = block {
          exit_if %b7
        }

        # False block
        %b6 = block {
          store %return_flag, true
          store %return_value, 0i
          exit_if %b7
        }

      # Merge block
      %b7 = block {
        %6:i32 = load %return_value
        ret %6
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Switch_ConditionalReturnInBody) {
    auto* global = b.Var(ty.ptr<private_, i32>());
    b.RootBlock()->Append(global);

    auto* cond = b.FunctionParam(ty.i32());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* sw = sb.Switch(cond);
    auto caseA = b.With(b.Case(sw, {Switch::CaseSelector{b.Constant(1_i)}}));
    auto* ifelse = caseA.If(cond);
    {
        auto tb = b.With(ifelse->True());
        tb.Return(func, 42_i);
        auto fb = b.With(ifelse->False());
        fb.ExitIf(ifelse);
    }
    caseA.Store(global, 2_i);
    caseA.ExitSwitch(sw);

    auto caseB = b.With(b.Case(sw, {Switch::CaseSelector{}}));
    caseB.ExitSwitch(sw);

    sb.Return(func, 0_i);

    auto* src = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:i32):i32 -> %b2 {
  %b2 = block {
    switch %3 [c: (1i, %b3), c: (default, %b4)]
      # Case block
      %b3 = block {
        if %3 [t: %b5, f: %b6]
          # True block
          %b5 = block {
            ret 42i
          }

          # False block
          %b6 = block {
            exit_if
          }

        store %1, 2i
        exit_switch
      }

      # Case block
      %b4 = block {
        exit_switch
      }

    ret 0i
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
# Root block
%b1 = block {
  %1:ptr<private, i32, read_write> = var
}

%foo = func(%3:i32):i32 -> %b2 {
  %b2 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    switch %3 [c: (1i, %b3), c: (default, %b4), m: %b5]
      # Case block
      %b3 = block {
        if %3 [t: %b6, f: %b7, m: %b8]
          # True block
          %b6 = block {
            store %return_flag, true
            store %return_value, 42i
            exit_if %b8
          }

          # False block
          %b7 = block {
            exit_if %b8
          }

        # Merge block
        %b8 = block {
          %6:bool = load %return_flag
          if %6 [t: %b9, f: %b10, m: %b11]
            # True block
            %b9 = block {
              exit_if %b11
            }

            # False block
            %b10 = block {
              store %1, 2i
              exit_switch %b5
            }

          # Merge block
          %b11 = block {
            exit_switch %b5
          }

        }

      }

      # Case block
      %b4 = block {
        exit_switch %b5
      }

    # Merge block
    %b5 = block {
      %7:bool = load %return_flag
      if %7 [t: %b12, f: %b13, m: %b14]
        # True block
        %b12 = block {
          exit_if %b14
        }

        # False block
        %b13 = block {
          store %return_flag, true
          store %return_value, 0i
          exit_if %b14
        }

      # Merge block
      %b14 = block {
        %8:i32 = load %return_value
        ret %8
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

TEST_F(IR_MergeReturnTest, Switch_WithBasicBlockArgumentsOnMerge) {
    auto* cond = b.FunctionParam(ty.i32());
    auto* func = b.Function("foo", ty.i32());
    func->SetParams({cond});
    mod.functions.Push(func);

    auto sb = b.With(func->StartTarget());

    auto* sw = sb.Switch(cond);
    sw->SetType(ty.tuple<i32>());
    auto caseA = b.With(b.Case(sw, {Switch::CaseSelector{b.Constant(1_i)}}));
    caseA.Return(func, 42_i);
    auto caseB = b.With(b.Case(sw, {Switch::CaseSelector{b.Constant(2_i)}}));
    caseB.Return(func, 99_i);
    auto caseC = b.With(b.Case(sw, {Switch::CaseSelector{b.Constant(3_i)}}));
    caseC.ExitSwitch(sw, 1_i);
    auto caseD = b.With(b.Case(sw, {Switch::CaseSelector{}}));
    caseD.ExitSwitch(sw, 0_i);

    sb.Return(func, sb.Access(ty.i32(), sw, 0_i));

    auto* src = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    switch %2 [c: (1i, %b2), c: (2i, %b3), c: (3i, %b4), c: (default, %b5)]
      # Case block
      %b2 = block {
        ret 42i
      }

      # Case block
      %b3 = block {
        ret 99i
      }

      # Case block
      %b4 = block {
        exit_switch 1i
      }

      # Case block
      %b5 = block {
        exit_switch 0i
      }

    %3:i32 = access %4, 0i
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%2:i32):i32 -> %b1 {
  %b1 = block {
    %return_value:ptr<function, i32, read_write> = var
    %return_flag:ptr<function, bool, read_write> = var, false
    switch %2 [c: (1i, %b2), c: (2i, %b3), c: (3i, %b4), c: (default, %b5), m: %b6]
      # Case block
      %b2 = block {
        store %return_flag, true
        store %return_value, 42i
        exit_switch %b6 undef
      }

      # Case block
      %b3 = block {
        store %return_flag, true
        store %return_value, 99i
        exit_switch %b6 undef
      }

      # Case block
      %b4 = block {
        exit_switch %b6 1i
      }

      # Case block
      %b5 = block {
        exit_switch %b6 0i
      }

    # Merge block
    %b6 = block (%5:i32) {
      %6:bool = load %return_flag
      if %6 [t: %b7, f: %b8, m: %b9]
        # True block
        %b7 = block {
          exit_if %b9
        }

        # False block
        %b8 = block {
          store %return_flag, true
          store %return_value, %5:i32
          exit_if %b9
        }

      # Merge block
      %b9 = block {
        %7:i32 = load %return_value
        ret %7
      }

    }

  }
}
)";

    // Run<MergeReturn>();

    (void)expect;  // EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::ir::transform
