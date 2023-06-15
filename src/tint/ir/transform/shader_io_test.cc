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

#include "src/tint/ir/transform/shader_io.h"

#include <utility>

#include "src/tint/ir/transform/test_helper.h"
#include "src/tint/type/struct.h"

namespace tint::ir::transform {
namespace {

using namespace tint::builtin::fluent_types;  // NOLINT
using namespace tint::number_suffixes;        // NOLINT

using IR_ShaderIOTest = TransformTest;

TEST_F(IR_ShaderIOTest, NoInputsOrOutputs) {
    auto* ep = b.Function("foo", ty.void_());
    ep->SetStage(Function::PipelineStage::kCompute);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    sb.Return(ep);

    auto* src = R"(
%foo = @compute func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ShaderIOTest, Parameters_NonStruct_Spirv) {
    auto* ep = b.Function("foo", ty.void_());
    auto* front_facing = b.FunctionParam(ty.bool_());
    front_facing->SetBuiltin(FunctionParam::Builtin::kFrontFacing);
    auto* position = b.FunctionParam(ty.vec4<f32>());
    position->SetBuiltin(FunctionParam::Builtin::kPosition);
    position->SetInvariant(true);
    auto* color1 = b.FunctionParam(ty.f32());
    color1->SetLocation(0, {});
    auto* color2 = b.FunctionParam(ty.f32());
    color2->SetLocation(1, builtin::Interpolation{builtin::InterpolationType::kLinear,
                                                  builtin::InterpolationSampling::kSample});
    ep->SetParams({front_facing, position, color1, color2});
    ep->SetStage(Function::PipelineStage::kFragment);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    auto* ifelse = sb.If(front_facing);
    {
        auto tb = b.With(ifelse->True());
        tb.Multiply(ty.vec4<f32>(), position, tb.Add(ty.f32(), color1, color2));
        tb.ExitIf(ifelse);
    }
    {
        auto mb = b.With(ifelse->Merge());
        mb.Return(ep);
    }

    auto* src = R"(
%foo = @fragment func(%2:bool [@front_facing], %3:vec4<f32> [@invariant, @position], %4:f32 [@location(0)], %5:f32 [@location(1), @interpolate(linear, sample)]):void -> %b1 {
  %b1 = block {
    if %2 [t: %b2, m: %b3]
      # True block
      %b2 = block {
        %6:f32 = add %4, %5
        %7:vec4<f32> = mul %3, %6
        exit_if %b3
      }

    # Merge block
    %b3 = block {
      ret
    }

  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
tint_symbol_4 = struct @align(16), @block {
  tint_symbol:bool @offset(0), @builtin(front_facing)
  tint_symbol_1:vec4<f32> @offset(16), @invariant, @builtin(position)
  tint_symbol_2:f32 @offset(32), @location(0)
  tint_symbol_3:f32 @offset(36), @location(1), @interpolate(linear, sample)
}

# Root block
%b1 = block {
  %1:ptr<__in, tint_symbol_4, read> = var
}

%foo_inner = func(%3:bool, %4:vec4<f32>, %5:f32, %6:f32):void -> %b2 {
  %b2 = block {
    if %3 [t: %b3, m: %b4]
      # True block
      %b3 = block {
        %7:f32 = add %5, %6
        %8:vec4<f32> = mul %4, %7
        exit_if %b4
      }

    # Merge block
    %b4 = block {
      ret
    }

  }
}
%foo = @fragment func():void -> %b5 {
  %b5 = block {
    %10:ptr<__in, bool, read> = access %1, 0u
    %11:bool = load %10
    %12:ptr<__in, vec4<f32>, read> = access %1, 1u
    %13:vec4<f32> = load %12
    %14:ptr<__in, f32, read> = access %1, 2u
    %15:f32 = load %14
    %16:ptr<__in, f32, read> = access %1, 3u
    %17:f32 = load %16
    %18:void = call %foo_inner, %11, %13, %15, %17
    ret
  }
}
)";

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ShaderIOTest, Parameters_Struct_Spirv) {
    auto* front_facing = ty.Get<type::StructMember>(
        mod.symbols.New("front_facing"), ty.bool_(), 0u, 0u, 1u, 1u,
        type::StructMemberAttributes{.builtin = builtin::BuiltinValue::kFrontFacing});
    auto* position = ty.Get<type::StructMember>(
        mod.symbols.New("position"), ty.vec4<f32>(), 1u, 16u, 16u, 16u,
        type::StructMemberAttributes{.builtin = builtin::BuiltinValue::kPosition,
                                     .invariant = true});
    auto* color1 = ty.Get<type::StructMember>(mod.symbols.New("color1"), ty.f32(), 2u, 32u, 4u, 4u,
                                              type::StructMemberAttributes{.location = 0u});
    auto* color2 = ty.Get<type::StructMember>(
        mod.symbols.New("color2"), ty.f32(), 3u, 36u, 4u, 4u,
        type::StructMemberAttributes{
            .location = 1u,
            .interpolation = builtin::Interpolation{builtin::InterpolationType::kLinear,
                                                    builtin::InterpolationSampling::kSample}});
    auto* str_ty =
        ty.Get<type::Struct>(mod.symbols.New("Inputs"),
                             utils::Vector{front_facing, position, color1, color2}, 16u, 48u, 40u);

    auto* ep = b.Function("foo", ty.void_());
    auto* str_param = b.FunctionParam(str_ty);
    ep->SetParams({str_param});
    ep->SetStage(Function::PipelineStage::kFragment);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    auto* ifelse = sb.If(sb.Access(ty.bool_(), str_param, 0_i));
    {
        auto tb = b.With(ifelse->True());
        tb.Multiply(ty.vec4<f32>(), tb.Access(ty.vec4<f32>(), str_param, 1_i),
                    tb.Add(ty.f32(), tb.Access(ty.f32(), str_param, 2_i),
                           tb.Access(ty.f32(), str_param, 3_i)));
        tb.ExitIf(ifelse);
    }
    {
        auto mb = b.With(ifelse->Merge());
        mb.Return(ep);
    }

    auto* src = R"(
Inputs = struct @align(16) {
  front_facing:bool @offset(0), @builtin(front_facing)
  position:vec4<f32> @offset(16), @invariant, @builtin(position)
  color1:f32 @offset(32), @location(0)
  color2:f32 @offset(36), @location(1), @interpolate(linear, sample)
}

%foo = @fragment func(%2:Inputs):void -> %b1 {
  %b1 = block {
    %3:bool = access %2, 0i
    if %3 [t: %b2, m: %b3]
      # True block
      %b2 = block {
        %4:vec4<f32> = access %2, 1i
        %5:f32 = access %2, 2i
        %6:f32 = access %2, 3i
        %7:f32 = add %5, %6
        %8:vec4<f32> = mul %4, %7
        exit_if %b3
      }

    # Merge block
    %b3 = block {
      ret
    }

  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inputs = struct @align(16) {
  front_facing:bool @offset(0)
  position:vec4<f32> @offset(16)
  color1:f32 @offset(32)
  color2:f32 @offset(36)
}

tint_symbol_4 = struct @align(16), @block {
  tint_symbol:bool @offset(0), @builtin(front_facing)
  tint_symbol_1:vec4<f32> @offset(16), @invariant, @builtin(position)
  tint_symbol_2:f32 @offset(32), @location(0)
  tint_symbol_3:f32 @offset(36), @location(1), @interpolate(linear, sample)
}

# Root block
%b1 = block {
  %1:ptr<__in, tint_symbol_4, read> = var
}

%foo_inner = func(%3:Inputs):void -> %b2 {
  %b2 = block {
    %4:bool = access %3, 0i
    if %4 [t: %b3, m: %b4]
      # True block
      %b3 = block {
        %5:vec4<f32> = access %3, 1i
        %6:f32 = access %3, 2i
        %7:f32 = access %3, 3i
        %8:f32 = add %6, %7
        %9:vec4<f32> = mul %5, %8
        exit_if %b4
      }

    # Merge block
    %b4 = block {
      ret
    }

  }
}
%foo = @fragment func():void -> %b5 {
  %b5 = block {
    %11:ptr<__in, bool, read> = access %1, 0u
    %12:bool = load %11
    %13:ptr<__in, vec4<f32>, read> = access %1, 1u
    %14:vec4<f32> = load %13
    %15:ptr<__in, f32, read> = access %1, 2u
    %16:f32 = load %15
    %17:ptr<__in, f32, read> = access %1, 3u
    %18:f32 = load %17
    %19:Inputs = construct %12, %14, %16, %18
    %20:void = call %foo_inner, %19
    ret
  }
}
)";

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ShaderIOTest, Parameters_Mixed_Spirv) {
    auto* position = ty.Get<type::StructMember>(
        mod.symbols.New("position"), ty.vec4<f32>(), 0u, 0u, 16u, 16u,
        type::StructMemberAttributes{.builtin = builtin::BuiltinValue::kPosition,
                                     .invariant = true});
    auto* color1 = ty.Get<type::StructMember>(mod.symbols.New("color1"), ty.f32(), 1u, 16u, 4u, 4u,
                                              type::StructMemberAttributes{.location = 0u});
    auto* str_ty = ty.Get<type::Struct>(mod.symbols.New("Inputs"), utils::Vector{position, color1},
                                        16u, 32u, 20u);

    auto* ep = b.Function("foo", ty.void_());
    auto* front_facing = b.FunctionParam(ty.bool_());
    front_facing->SetBuiltin(FunctionParam::Builtin::kFrontFacing);
    auto* str_param = b.FunctionParam(str_ty);
    auto* color2 = b.FunctionParam(ty.f32());
    color2->SetLocation(1, builtin::Interpolation{builtin::InterpolationType::kLinear,
                                                  builtin::InterpolationSampling::kSample});
    ep->SetParams({front_facing, str_param, color2});
    ep->SetStage(Function::PipelineStage::kFragment);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    auto* ifelse = sb.If(front_facing);
    {
        auto tb = b.With(ifelse->True());
        tb.Multiply(ty.vec4<f32>(), tb.Access(ty.vec4<f32>(), str_param, 0_i),
                    tb.Add(ty.f32(), tb.Access(ty.f32(), str_param, 1_i), color2));
        tb.ExitIf(ifelse);
    }
    {
        auto mb = b.With(ifelse->Merge());
        mb.Return(ep);
    }

    auto* src = R"(
Inputs = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color1:f32 @offset(16), @location(0)
}

%foo = @fragment func(%2:bool [@front_facing], %3:Inputs, %4:f32 [@location(1), @interpolate(linear, sample)]):void -> %b1 {
  %b1 = block {
    if %2 [t: %b2, m: %b3]
      # True block
      %b2 = block {
        %5:vec4<f32> = access %3, 0i
        %6:f32 = access %3, 1i
        %7:f32 = add %6, %4
        %8:vec4<f32> = mul %5, %7
        exit_if %b3
      }

    # Merge block
    %b3 = block {
      ret
    }

  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Inputs = struct @align(16) {
  position:vec4<f32> @offset(0)
  color1:f32 @offset(16)
}

tint_symbol_4 = struct @align(16), @block {
  tint_symbol:bool @offset(0), @builtin(front_facing)
  tint_symbol_1:vec4<f32> @offset(16), @invariant, @builtin(position)
  tint_symbol_2:f32 @offset(32), @location(0)
  tint_symbol_3:f32 @offset(36), @location(1), @interpolate(linear, sample)
}

# Root block
%b1 = block {
  %1:ptr<__in, tint_symbol_4, read> = var
}

%foo_inner = func(%3:bool, %4:Inputs, %5:f32):void -> %b2 {
  %b2 = block {
    if %3 [t: %b3, m: %b4]
      # True block
      %b3 = block {
        %6:vec4<f32> = access %4, 0i
        %7:f32 = access %4, 1i
        %8:f32 = add %7, %5
        %9:vec4<f32> = mul %6, %8
        exit_if %b4
      }

    # Merge block
    %b4 = block {
      ret
    }

  }
}
%foo = @fragment func():void -> %b5 {
  %b5 = block {
    %11:ptr<__in, bool, read> = access %1, 0u
    %12:bool = load %11
    %13:ptr<__in, vec4<f32>, read> = access %1, 1u
    %14:vec4<f32> = load %13
    %15:ptr<__in, f32, read> = access %1, 2u
    %16:f32 = load %15
    %17:Inputs = construct %14, %16
    %18:ptr<__in, f32, read> = access %1, 3u
    %19:f32 = load %18
    %20:void = call %foo_inner, %12, %17, %19
    ret
  }
}
)";

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ShaderIOTest, ReturnValue_NonStructBuiltin_Spirv) {
    auto* ep = b.Function("foo", ty.vec4<f32>());
    ep->SetReturnBuiltin(Function::ReturnBuiltin::kPosition);
    ep->SetReturnInvariant(true);
    ep->SetStage(Function::PipelineStage::kVertex);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    sb.Return(ep, sb.Construct(ty.vec4<f32>(), 0.5_f));

    auto* src = R"(
%foo = @vertex func():vec4<f32> [@invariant, @position] -> %b1 {
  %b1 = block {
    %2:vec4<f32> = construct 0.5f
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
tint_symbol_1 = struct @align(16), @block {
  tint_symbol:vec4<f32> @offset(0), @invariant, @builtin(position)
}

# Root block
%b1 = block {
  %1:ptr<__out, tint_symbol_1, write> = var
}

%foo_inner = func():vec4<f32> -> %b2 {
  %b2 = block {
    %3:vec4<f32> = construct 0.5f
    ret %3
  }
}
%foo = @vertex func():void -> %b3 {
  %b3 = block {
    %5:vec4<f32> = call %foo_inner
    %6:ptr<__out, vec4<f32>, write> = access %1, 0u
    store %6, %5
    ret
  }
}
)";

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ShaderIOTest, ReturnValue_NonStructLocation_Spirv) {
    auto* ep = b.Function("foo", ty.vec4<f32>());
    ep->SetReturnLocation(1u, {});
    ep->SetStage(Function::PipelineStage::kFragment);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    sb.Return(ep, sb.Construct(ty.vec4<f32>(), 0.5_f));

    auto* src = R"(
%foo = @fragment func():vec4<f32> [@location(1)] -> %b1 {
  %b1 = block {
    %2:vec4<f32> = construct 0.5f
    ret %2
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
tint_symbol_1 = struct @align(16), @block {
  tint_symbol:vec4<f32> @offset(0), @location(1)
}

# Root block
%b1 = block {
  %1:ptr<__out, tint_symbol_1, write> = var
}

%foo_inner = func():vec4<f32> -> %b2 {
  %b2 = block {
    %3:vec4<f32> = construct 0.5f
    ret %3
  }
}
%foo = @fragment func():void -> %b3 {
  %b3 = block {
    %5:vec4<f32> = call %foo_inner
    %6:ptr<__out, vec4<f32>, write> = access %1, 0u
    store %6, %5
    ret
  }
}
)";

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

TEST_F(IR_ShaderIOTest, ReturnValue_Struct_Spirv) {
    auto* position = ty.Get<type::StructMember>(
        mod.symbols.New("position"), ty.vec4<f32>(), 0u, 0u, 16u, 16u,
        type::StructMemberAttributes{.builtin = builtin::BuiltinValue::kPosition,
                                     .invariant = true});
    auto* color1 = ty.Get<type::StructMember>(mod.symbols.New("color1"), ty.f32(), 1u, 16u, 4u, 4u,
                                              type::StructMemberAttributes{.location = 0u});
    auto* color2 = ty.Get<type::StructMember>(
        mod.symbols.New("color2"), ty.f32(), 2u, 20u, 4u, 4u,
        type::StructMemberAttributes{
            .location = 1u,
            .interpolation = builtin::Interpolation{builtin::InterpolationType::kLinear,
                                                    builtin::InterpolationSampling::kSample}});
    auto* str_ty = ty.Get<type::Struct>(mod.symbols.New("Outputs"),
                                        utils::Vector{position, color1, color2}, 16u, 32u, 24u);

    auto* ep = b.Function("foo", str_ty);
    ep->SetStage(Function::PipelineStage::kVertex);
    mod.functions.Push(ep);

    auto sb = b.With(ep->StartTarget());
    sb.Return(ep, sb.Construct(str_ty, sb.Construct(ty.vec4<f32>(), 0_f), 0.25_f, 0.75_f));

    auto* src = R"(
Outputs = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color1:f32 @offset(16), @location(0)
  color2:f32 @offset(20), @location(1), @interpolate(linear, sample)
}

%foo = @vertex func():Outputs -> %b1 {
  %b1 = block {
    %2:vec4<f32> = construct 0.0f
    %3:Outputs = construct %2, 0.25f, 0.75f
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
Outputs = struct @align(16) {
  position:vec4<f32> @offset(0)
  color1:f32 @offset(16)
  color2:f32 @offset(20)
}

tint_symbol_3 = struct @align(16), @block {
  tint_symbol:vec4<f32> @offset(0), @invariant, @builtin(position)
  tint_symbol_1:f32 @offset(16), @location(0)
  tint_symbol_2:f32 @offset(20), @location(1), @interpolate(linear, sample)
}

# Root block
%b1 = block {
  %1:ptr<__out, tint_symbol_3, write> = var
}

%foo_inner = func():Outputs -> %b2 {
  %b2 = block {
    %3:vec4<f32> = construct 0.0f
    %4:Outputs = construct %3, 0.25f, 0.75f
    ret %4
  }
}
%foo = @vertex func():void -> %b3 {
  %b3 = block {
    %6:Outputs = call %foo_inner
    %7:vec4<f32> = access %6, 0u
    %8:ptr<__out, vec4<f32>, write> = access %1, 0u
    store %8, %7
    %9:f32 = access %6, 1u
    %10:ptr<__out, f32, write> = access %1, 1u
    store %10, %9
    %11:f32 = access %6, 2u
    %12:ptr<__out, f32, write> = access %1, 2u
    store %12, %11
    ret
  }
}
)";

    Transform::DataMap data;
    data.Add<ShaderIO::Config>(ShaderIO::Backend::kSpirv);
    Run<ShaderIO>(data);

    EXPECT_EQ(expect, str());
}

// TODO: struct used for other things

}  // namespace
}  // namespace tint::ir::transform
