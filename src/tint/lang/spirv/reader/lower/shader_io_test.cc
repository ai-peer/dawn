// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/spirv/reader/lower/shader_io.h"

#include <utility>

#include "src/tint/lang/core/ir/transform/helper_test.h"

namespace tint::spirv::reader::lower {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

using SpirvReader_ShaderIOTest = core::ir::transform::TransformTest;

TEST_F(SpirvReader_ShaderIOTest, NoInputsOrOutputs) {
    auto* ep = b.Function("foo", ty.void_());
    ep->SetStage(core::ir::Function::PipelineStage::kCompute);

    b.Append(ep->Block(), [&] {  //
        b.Return(ep);
    });

    auto* src = R"(
%foo = @compute func():void -> %b1 {
  %b1 = block {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Inputs) {
    auto* front_facing = b.Var("front_facing", ty.ptr(core::AddressSpace::kIn, ty.bool_()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kFrontFacing;
        front_facing->SetAttributes(std::move(attributes));
    }
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kIn, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        attributes.invariant = true;
        position->SetAttributes(std::move(attributes));
    }
    auto* color1 = b.Var("color1", ty.ptr(core::AddressSpace::kIn, ty.f32()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 0;
        color1->SetAttributes(std::move(attributes));
    }
    auto* color2 = b.Var("color2", ty.ptr(core::AddressSpace::kIn, ty.f32()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1;
        attributes.interpolation = core::Interpolation{core::InterpolationType::kLinear,
                                                       core::InterpolationSampling::kSample};
        color2->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(front_facing);
    mod.root_block->Append(position);
    mod.root_block->Append(color1);
    mod.root_block->Append(color2);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {
        auto* ifelse = b.If(b.Load(front_facing));
        b.Append(ifelse->True(), [&] {
            auto* position_value = b.Load(position);
            auto* color1_value = b.Load(color1);
            auto* color2_value = b.Load(color2);
            b.Multiply(ty.vec4<f32>(), position_value, b.Add(ty.f32(), color1_value, color2_value));
            b.ExitIf(ifelse);
        });
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %front_facing:ptr<__in, bool, read_write> = var @builtin(front_facing)
  %position:ptr<__in, vec4<f32>, read_write> = var @invariant @builtin(position)
  %color1:ptr<__in, f32, read_write> = var @location(0)
  %color2:ptr<__in, f32, read_write> = var @location(1) @interpolate(linear, sample)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    %6:bool = load %front_facing
    if %6 [t: %b3] {  # if_1
      %b3 = block {  # true
        %7:vec4<f32> = load %position
        %8:f32 = load %color1
        %9:f32 = load %color2
        %10:f32 = add %8, %9
        %11:vec4<f32> = mul %7, %10
        exit_if  # if_1
      }
    }
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = @fragment func(%front_facing:bool [@front_facing], %position:vec4<f32> [@invariant, @position], %color1:f32 [@location(0)], %color2:f32 [@location(1), @interpolate(linear, sample)]):void -> %b1 {
  %b1 = block {
    if %front_facing [t: %b2] {  # if_1
      %b2 = block {  # true
        %6:f32 = add %color1, %color2
        %7:vec4<f32> = mul %position, %6
        exit_if  # if_1
      }
    }
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Inputs_UsedByHelper) {
    auto* front_facing = b.Var("front_facing", ty.ptr(core::AddressSpace::kIn, ty.bool_()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kFrontFacing;
        front_facing->SetAttributes(std::move(attributes));
    }
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kIn, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        attributes.invariant = true;
        position->SetAttributes(std::move(attributes));
    }
    auto* color1 = b.Var("color1", ty.ptr(core::AddressSpace::kIn, ty.f32()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 0;
        color1->SetAttributes(std::move(attributes));
    }
    auto* color2 = b.Var("color2", ty.ptr(core::AddressSpace::kIn, ty.f32()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1;
        attributes.interpolation = core::Interpolation{core::InterpolationType::kLinear,
                                                       core::InterpolationSampling::kSample};
        color2->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(front_facing);
    mod.root_block->Append(position);
    mod.root_block->Append(color1);
    mod.root_block->Append(color2);

    // Inner function has an existing parameter.
    auto* param = b.FunctionParam("existing_param", ty.f32());
    auto* foo = b.Function("foo", ty.void_());
    foo->SetParams({param});
    b.Append(foo->Block(), [&] {
        auto* ifelse = b.If(b.Load(front_facing));
        b.Append(ifelse->True(), [&] {
            auto* position_value = b.Load(position);
            auto* color1_value = b.Load(color1);
            auto* color2_value = b.Load(color2);
            auto* add = b.Add(ty.f32(), color1_value, color2_value);
            auto* mul = b.Multiply(ty.vec4<f32>(), position_value, add);
            b.Divide(ty.vec4<f32>(), mul, param);
            b.ExitIf(ifelse);
        });
        b.Return(foo);
    });

    // Intermediate function has no existing parameters.
    auto* bar = b.Function("bar", ty.void_());
    b.Append(bar->Block(), [&] {
        b.Call(foo, 42_f);
        b.Return(bar);
    });

    auto* ep = b.Function("main", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {
        b.Call(bar);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %front_facing:ptr<__in, bool, read_write> = var @builtin(front_facing)
  %position:ptr<__in, vec4<f32>, read_write> = var @invariant @builtin(position)
  %color1:ptr<__in, f32, read_write> = var @location(0)
  %color2:ptr<__in, f32, read_write> = var @location(1) @interpolate(linear, sample)
}

%foo = func(%existing_param:f32):void -> %b2 {
  %b2 = block {
    %7:bool = load %front_facing
    if %7 [t: %b3] {  # if_1
      %b3 = block {  # true
        %8:vec4<f32> = load %position
        %9:f32 = load %color1
        %10:f32 = load %color2
        %11:f32 = add %9, %10
        %12:vec4<f32> = mul %8, %11
        %13:vec4<f32> = div %12, %existing_param
        exit_if  # if_1
      }
    }
    ret
  }
}
%bar = func():void -> %b4 {
  %b4 = block {
    %15:void = call %foo, 42.0f
    ret
  }
}
%main = @fragment func():void -> %b5 {
  %b5 = block {
    %17:void = call %bar
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%existing_param:f32, %front_facing:bool, %position:vec4<f32>, %color1:f32, %color2:f32):void -> %b1 {
  %b1 = block {
    if %front_facing [t: %b2] {  # if_1
      %b2 = block {  # true
        %7:f32 = add %color1, %color2
        %8:vec4<f32> = mul %position, %7
        %9:vec4<f32> = div %8, %existing_param
        exit_if  # if_1
      }
    }
    ret
  }
}
%bar = func(%front_facing_1:bool, %position_1:vec4<f32>, %color1_1:f32, %color2_1:f32):void -> %b3 {  # %front_facing_1: 'front_facing', %position_1: 'position', %color1_1: 'color1', %color2_1: 'color2'
  %b3 = block {
    %15:void = call %foo, 42.0f, %front_facing_1, %position_1, %color1_1, %color2_1
    ret
  }
}
%main = @fragment func(%front_facing_2:bool [@front_facing], %position_2:vec4<f32> [@invariant, @position], %color1_2:f32 [@location(0)], %color2_2:f32 [@location(1), @interpolate(linear, sample)]):void -> %b4 {  # %front_facing_2: 'front_facing', %position_2: 'position', %color1_2: 'color1', %color2_2: 'color2'
  %b4 = block {
    %21:void = call %bar, %front_facing_2, %position_2, %color1_2, %color2_2
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Inputs_UsedEntryPointAndHelper) {
    auto* gid = b.Var("gid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kGlobalInvocationId;
        gid->SetAttributes(std::move(attributes));
    }
    auto* lid = b.Var("lid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kLocalInvocationId;
        lid->SetAttributes(std::move(attributes));
    }
    auto* group_id = b.Var("group_id", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kWorkgroupId;
        group_id->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(gid);
    mod.root_block->Append(lid);
    mod.root_block->Append(group_id);

    // Use a subset of the inputs in the helper.
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* gid_value = b.Load(gid);
        auto* lid_value = b.Load(lid);
        b.Add(ty.vec3<u32>(), gid_value, lid_value);
        b.Return(foo);
    });

    // Use a different subset of the inputs in the entry point.
    auto* ep = b.Function("main1", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    b.Append(ep->Block(), [&] {
        auto* group_value = b.Load(group_id);
        auto* gid_value = b.Load(gid);
        b.Add(ty.vec3<u32>(), group_value, gid_value);
        b.Call(foo);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %gid:ptr<__in, vec3<u32>, read_write> = var @builtin(global_invocation_id)
  %lid:ptr<__in, vec3<u32>, read_write> = var @builtin(local_invocation_id)
  %group_id:ptr<__in, vec3<u32>, read_write> = var @builtin(workgroup_id)
}

%foo = func():void -> %b2 {
  %b2 = block {
    %5:vec3<u32> = load %gid
    %6:vec3<u32> = load %lid
    %7:vec3<u32> = add %5, %6
    ret
  }
}
%main1 = @compute func():void -> %b3 {
  %b3 = block {
    %9:vec3<u32> = load %group_id
    %10:vec3<u32> = load %gid
    %11:vec3<u32> = add %9, %10
    %12:void = call %foo
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%gid:vec3<u32>, %lid:vec3<u32>):void -> %b1 {
  %b1 = block {
    %4:vec3<u32> = add %gid, %lid
    ret
  }
}
%main1 = @compute func(%gid_1:vec3<u32> [@global_invocation_id], %lid_1:vec3<u32> [@local_invocation_id], %group_id:vec3<u32> [@workgroup_id]):void -> %b2 {  # %gid_1: 'gid', %lid_1: 'lid'
  %b2 = block {
    %9:vec3<u32> = add %group_id, %gid_1
    %10:void = call %foo, %gid_1, %lid_1
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Inputs_UsedEntryPointAndHelper_ForwardReference) {
    auto* gid = b.Var("gid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kGlobalInvocationId;
        gid->SetAttributes(std::move(attributes));
    }
    auto* lid = b.Var("lid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kLocalInvocationId;
        lid->SetAttributes(std::move(attributes));
    }
    auto* group_id = b.Var("group_id", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kWorkgroupId;
        group_id->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(gid);
    mod.root_block->Append(lid);
    mod.root_block->Append(group_id);

    auto* ep = b.Function("main1", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    auto* foo = b.Function("foo", ty.void_());

    // Use a subset of the inputs in the entry point.
    b.Append(ep->Block(), [&] {
        auto* group_value = b.Load(group_id);
        auto* gid_value = b.Load(gid);
        b.Add(ty.vec3<u32>(), group_value, gid_value);
        b.Call(foo);
        b.Return(ep);
    });

    // Use a different subset of the variables in the helper.
    b.Append(foo->Block(), [&] {
        auto* gid_value = b.Load(gid);
        auto* lid_value = b.Load(lid);
        b.Add(ty.vec3<u32>(), gid_value, lid_value);
        b.Return(foo);
    });

    auto* src = R"(
%b1 = block {  # root
  %gid:ptr<__in, vec3<u32>, read_write> = var @builtin(global_invocation_id)
  %lid:ptr<__in, vec3<u32>, read_write> = var @builtin(local_invocation_id)
  %group_id:ptr<__in, vec3<u32>, read_write> = var @builtin(workgroup_id)
}

%main1 = @compute func():void -> %b2 {
  %b2 = block {
    %5:vec3<u32> = load %group_id
    %6:vec3<u32> = load %gid
    %7:vec3<u32> = add %5, %6
    %8:void = call %foo
    ret
  }
}
%foo = func():void -> %b3 {
  %b3 = block {
    %10:vec3<u32> = load %gid
    %11:vec3<u32> = load %lid
    %12:vec3<u32> = add %10, %11
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%main1 = @compute func(%gid:vec3<u32> [@global_invocation_id], %lid:vec3<u32> [@local_invocation_id], %group_id:vec3<u32> [@workgroup_id]):void -> %b1 {
  %b1 = block {
    %5:vec3<u32> = add %group_id, %gid
    %6:void = call %foo, %gid, %lid
    ret
  }
}
%foo = func(%gid_1:vec3<u32>, %lid_1:vec3<u32>):void -> %b2 {  # %gid_1: 'gid', %lid_1: 'lid'
  %b2 = block {
    %10:vec3<u32> = add %gid_1, %lid_1
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Inputs_UsedByMultipleEntryPoints) {
    auto* gid = b.Var("gid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kGlobalInvocationId;
        gid->SetAttributes(std::move(attributes));
    }
    auto* lid = b.Var("lid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kLocalInvocationId;
        lid->SetAttributes(std::move(attributes));
    }
    auto* group_id = b.Var("group_id", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kWorkgroupId;
        group_id->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(gid);
    mod.root_block->Append(lid);
    mod.root_block->Append(group_id);

    // Use a subset of the inputs in the helper.
    auto* foo = b.Function("foo", ty.void_());
    b.Append(foo->Block(), [&] {
        auto* gid_value = b.Load(gid);
        auto* lid_value = b.Load(lid);
        b.Add(ty.vec3<u32>(), gid_value, lid_value);
        b.Return(foo);
    });

    // Call the helper without directly referencing any inputs.
    auto* ep1 = b.Function("main1", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    b.Append(ep1->Block(), [&] {
        b.Call(foo);
        b.Return(ep1);
    });

    // Reference another input and then call the helper.
    auto* ep2 = b.Function("main2", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    b.Append(ep2->Block(), [&] {
        auto* group_value = b.Load(group_id);
        b.Add(ty.vec3<u32>(), group_value, group_value);
        b.Call(foo);
        b.Return(ep1);
    });

    auto* src = R"(
%b1 = block {  # root
  %gid:ptr<__in, vec3<u32>, read_write> = var @builtin(global_invocation_id)
  %lid:ptr<__in, vec3<u32>, read_write> = var @builtin(local_invocation_id)
  %group_id:ptr<__in, vec3<u32>, read_write> = var @builtin(workgroup_id)
}

%foo = func():void -> %b2 {
  %b2 = block {
    %5:vec3<u32> = load %gid
    %6:vec3<u32> = load %lid
    %7:vec3<u32> = add %5, %6
    ret
  }
}
%main1 = @compute func():void -> %b3 {
  %b3 = block {
    %9:void = call %foo
    ret
  }
}
%main2 = @compute func():void -> %b4 {
  %b4 = block {
    %11:vec3<u32> = load %group_id
    %12:vec3<u32> = add %11, %11
    %13:void = call %foo
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = func(%gid:vec3<u32>, %lid:vec3<u32>):void -> %b1 {
  %b1 = block {
    %4:vec3<u32> = add %gid, %lid
    ret
  }
}
%main1 = @compute func(%gid_1:vec3<u32> [@global_invocation_id], %lid_1:vec3<u32> [@local_invocation_id]):void -> %b2 {  # %gid_1: 'gid', %lid_1: 'lid'
  %b2 = block {
    %8:void = call %foo, %gid_1, %lid_1
    ret
  }
}
%main2 = @compute func(%gid_2:vec3<u32> [@global_invocation_id], %lid_2:vec3<u32> [@local_invocation_id], %group_id:vec3<u32> [@workgroup_id]):void -> %b3 {  # %gid_2: 'gid', %lid_2: 'lid'
  %b3 = block {
    %13:vec3<u32> = add %group_id, %group_id
    %14:void = call %foo, %gid_2, %lid_2
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Input_LoadVectorElement) {
    auto* lid = b.Var("lid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kLocalInvocationId;
        lid->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(lid);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    b.Append(ep->Block(), [&] {
        b.LoadVectorElement(lid, 2_u);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %lid:ptr<__in, vec3<u32>, read_write> = var @builtin(local_invocation_id)
}

%foo = @compute func():void -> %b2 {
  %b2 = block {
    %3:u32 = load_vector_element %lid, 2u
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = @compute func(%lid:vec3<u32> [@local_invocation_id]):void -> %b1 {
  %b1 = block {
    %3:u32 = access %lid, 2u
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Input_AccessChains) {
    auto* lid = b.Var("lid", ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kLocalInvocationId;
        lid->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(lid);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kCompute);
    b.Append(ep->Block(), [&] {
        auto* access_1 = b.Access(ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()), lid);
        auto* access_2 = b.Access(ty.ptr(core::AddressSpace::kIn, ty.vec3<u32>()), access_1);
        auto* vec = b.Load(access_2);
        auto* z = b.LoadVectorElement(access_2, 2_u);
        b.Multiply<vec3<u32>>(vec, z);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %lid:ptr<__in, vec3<u32>, read_write> = var @builtin(local_invocation_id)
}

%foo = @compute func():void -> %b2 {
  %b2 = block {
    %3:ptr<__in, vec3<u32>, read_write> = access %lid
    %4:ptr<__in, vec3<u32>, read_write> = access %3
    %5:vec3<u32> = load %4
    %6:u32 = load_vector_element %4, 2u
    %7:vec3<u32> = mul %5, %6
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%foo = @compute func(%lid:vec3<u32> [@local_invocation_id]):void -> %b1 {
  %b1 = block {
    %3:u32 = access %lid, 2u
    %4:vec3<u32> = mul %lid, %3
    ret
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, SingleOutput_Builtin) {
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        position->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(position);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kVertex);
    b.Append(ep->Block(), [&] {  //
        b.Store(position, b.Splat<vec4<f32>>(1_f, 4));
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %position:ptr<__out, vec4<f32>, read_write> = var @builtin(position)
}

%foo = @vertex func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %position:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    ret
  }
}
%foo = @vertex func():vec4<f32> [@position] -> %b3 {
  %b3 = block {
    %4:void = call %foo_inner
    %5:vec4<f32> = load %position
    ret %5
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, SingleOutput_Builtin_WithInvariant) {
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        attributes.invariant = true;
        position->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(position);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kVertex);
    b.Append(ep->Block(), [&] {  //
        b.Store(position, b.Splat<vec4<f32>>(1_f, 4));
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %position:ptr<__out, vec4<f32>, read_write> = var @invariant @builtin(position)
}

%foo = @vertex func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %position:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    ret
  }
}
%foo = @vertex func():vec4<f32> [@invariant, @position] -> %b3 {
  %b3 = block {
    %4:void = call %foo_inner
    %5:vec4<f32> = load %position
    ret %5
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, SingleOutput_Location) {
    auto* color = b.Var("color", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        color->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(color);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {  //
        b.Store(color, b.Splat<vec4<f32>>(1_f, 4));
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %color:ptr<__out, vec4<f32>, read_write> = var @location(1)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %color:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    ret
  }
}
%foo = @fragment func():vec4<f32> [@location(1)] -> %b3 {
  %b3 = block {
    %4:void = call %foo_inner
    %5:vec4<f32> = load %color
    ret %5
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, SingleOutput_Location_WithInterpolation) {
    auto* color = b.Var("color", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        attributes.interpolation = core::Interpolation{core::InterpolationType::kPerspective,
                                                       core::InterpolationSampling::kCentroid};
        color->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(color);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {  //
        b.Store(color, b.Splat<vec4<f32>>(1_f, 4));
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %color:ptr<__out, vec4<f32>, read_write> = var @location(1) @interpolate(perspective, centroid)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %color:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    ret
  }
}
%foo = @fragment func():vec4<f32> [@location(1), @interpolate(perspective, centroid)] -> %b3 {
  %b3 = block {
    %4:void = call %foo_inner
    %5:vec4<f32> = load %color
    ret %5
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, MultipleOutputs) {
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        attributes.invariant = true;
        position->SetAttributes(std::move(attributes));
    }
    auto* color1 = b.Var("color1", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        color1->SetAttributes(std::move(attributes));
    }
    auto* color2 = b.Var("color2", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        attributes.interpolation = core::Interpolation{core::InterpolationType::kPerspective,
                                                       core::InterpolationSampling::kCentroid};
        color2->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(position);
    mod.root_block->Append(color1);
    mod.root_block->Append(color2);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kVertex);
    b.Append(ep->Block(), [&] {  //
        b.Store(position, b.Splat<vec4<f32>>(1_f, 4));
        b.Store(color1, b.Splat<vec4<f32>>(0.5_f, 4));
        b.Store(color2, b.Splat<vec4<f32>>(0.25_f, 4));
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %position:ptr<__out, vec4<f32>, read_write> = var @invariant @builtin(position)
  %color1:ptr<__out, vec4<f32>, read_write> = var @location(1)
  %color2:ptr<__out, vec4<f32>, read_write> = var @location(1) @interpolate(perspective, centroid)
}

%foo = @vertex func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    store %color1, vec4<f32>(0.5f)
    store %color2, vec4<f32>(0.25f)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
tint_symbol = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color1:vec4<f32> @offset(16), @location(1)
  color2:vec4<f32> @offset(32), @location(1), @interpolate(perspective, centroid)
}

%b1 = block {  # root
  %position:ptr<private, vec4<f32>, read_write> = var
  %color1:ptr<private, vec4<f32>, read_write> = var
  %color2:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    store %color1, vec4<f32>(0.5f)
    store %color2, vec4<f32>(0.25f)
    ret
  }
}
%foo = @vertex func():tint_symbol -> %b3 {
  %b3 = block {
    %6:void = call %foo_inner
    %7:vec4<f32> = load %position
    %8:vec4<f32> = load %color1
    %9:vec4<f32> = load %color2
    %10:tint_symbol = construct %7, %8, %9
    ret %10
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Outputs_UsedByMultipleEntryPoints) {
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        attributes.invariant = true;
        position->SetAttributes(std::move(attributes));
    }
    auto* color1 = b.Var("color1", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        color1->SetAttributes(std::move(attributes));
    }
    auto* color2 = b.Var("color2", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        attributes.interpolation = core::Interpolation{core::InterpolationType::kPerspective,
                                                       core::InterpolationSampling::kCentroid};
        color2->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(position);
    mod.root_block->Append(color1);
    mod.root_block->Append(color2);

    auto* ep1 = b.Function("main1", ty.void_(), core::ir::Function::PipelineStage::kVertex);
    b.Append(ep1->Block(), [&] {  //
        b.Store(position, b.Splat<vec4<f32>>(1_f, 4));
        b.Return(ep1);
    });

    auto* ep2 = b.Function("main2", ty.void_(), core::ir::Function::PipelineStage::kVertex);
    b.Append(ep2->Block(), [&] {  //
        b.Store(position, b.Splat<vec4<f32>>(1_f, 4));
        b.Store(color1, b.Splat<vec4<f32>>(0.5_f, 4));
        b.Return(ep2);
    });

    auto* ep3 = b.Function("main3", ty.void_(), core::ir::Function::PipelineStage::kVertex);
    b.Append(ep3->Block(), [&] {  //
        b.Store(position, b.Splat<vec4<f32>>(1_f, 4));
        b.Store(color2, b.Splat<vec4<f32>>(0.25_f, 4));
        b.Return(ep3);
    });

    auto* src = R"(
%b1 = block {  # root
  %position:ptr<__out, vec4<f32>, read_write> = var @invariant @builtin(position)
  %color1:ptr<__out, vec4<f32>, read_write> = var @location(1)
  %color2:ptr<__out, vec4<f32>, read_write> = var @location(1) @interpolate(perspective, centroid)
}

%main1 = @vertex func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    ret
  }
}
%main2 = @vertex func():void -> %b3 {
  %b3 = block {
    store %position, vec4<f32>(1.0f)
    store %color1, vec4<f32>(0.5f)
    ret
  }
}
%main3 = @vertex func():void -> %b4 {
  %b4 = block {
    store %position, vec4<f32>(1.0f)
    store %color2, vec4<f32>(0.25f)
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
tint_symbol = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color1:vec4<f32> @offset(16), @location(1)
}

tint_symbol_1 = struct @align(16) {
  position:vec4<f32> @offset(0), @invariant, @builtin(position)
  color2:vec4<f32> @offset(16), @location(1), @interpolate(perspective, centroid)
}

%b1 = block {  # root
  %position:ptr<private, vec4<f32>, read_write> = var
  %color1:ptr<private, vec4<f32>, read_write> = var
  %color2:ptr<private, vec4<f32>, read_write> = var
}

%main1_inner = func():void -> %b2 {
  %b2 = block {
    store %position, vec4<f32>(1.0f)
    ret
  }
}
%main2_inner = func():void -> %b3 {
  %b3 = block {
    store %position, vec4<f32>(1.0f)
    store %color1, vec4<f32>(0.5f)
    ret
  }
}
%main3_inner = func():void -> %b4 {
  %b4 = block {
    store %position, vec4<f32>(1.0f)
    store %color2, vec4<f32>(0.25f)
    ret
  }
}
%main1 = @vertex func():vec4<f32> [@invariant, @position] -> %b5 {
  %b5 = block {
    %8:void = call %main1_inner
    %9:vec4<f32> = load %position
    ret %9
  }
}
%main2 = @vertex func():tint_symbol -> %b6 {
  %b6 = block {
    %11:void = call %main2_inner
    %12:vec4<f32> = load %position
    %13:vec4<f32> = load %color1
    %14:tint_symbol = construct %12, %13
    ret %14
  }
}
%main3 = @vertex func():tint_symbol_1 -> %b7 {
  %b7 = block {
    %16:void = call %main3_inner
    %17:vec4<f32> = load %position
    %18:vec4<f32> = load %color2
    %19:tint_symbol_1 = construct %17, %18
    ret %19
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Output_LoadAndStore) {
    auto* color = b.Var("color", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        color->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(color);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {  //
        b.Store(color, b.Splat<vec4<f32>>(1_f, 4));
        auto* load = b.Load(color);
        auto* mul = b.Multiply<vec4<f32>>(load, 2_f);
        b.Store(color, mul);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %color:ptr<__out, vec4<f32>, read_write> = var @location(1)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    %3:vec4<f32> = load %color
    %4:vec4<f32> = mul %3, 2.0f
    store %color, %4
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %color:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    %3:vec4<f32> = load %color
    %4:vec4<f32> = mul %3, 2.0f
    store %color, %4
    ret
  }
}
%foo = @fragment func():vec4<f32> [@location(1)] -> %b3 {
  %b3 = block {
    %6:void = call %foo_inner
    %7:vec4<f32> = load %color
    ret %7
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Output_LoadVectorElementAndStoreVectorElement) {
    auto* color = b.Var("color", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        color->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(color);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {  //
        b.Store(color, b.Splat<vec4<f32>>(1_f, 4));
        auto* load = b.LoadVectorElement(color, 2_u);
        auto* mul = b.Multiply<f32>(load, 2_f);
        b.StoreVectorElement(color, 2_u, mul);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %color:ptr<__out, vec4<f32>, read_write> = var @location(1)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    %3:f32 = load_vector_element %color, 2u
    %4:f32 = mul %3, 2.0f
    store_vector_element %color, 2u, %4
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %color:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    store %color, vec4<f32>(1.0f)
    %3:f32 = load_vector_element %color, 2u
    %4:f32 = mul %3, 2.0f
    store_vector_element %color, 2u, %4
    ret
  }
}
%foo = @fragment func():vec4<f32> [@location(1)] -> %b3 {
  %b3 = block {
    %6:void = call %foo_inner
    %7:vec4<f32> = load %color
    ret %7
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Output_AccessChain) {
    auto* color = b.Var("color", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1u;
        color->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(color);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {  //
        auto* access_1 = b.Access(ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()), color);
        auto* access_2 = b.Access(ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()), access_1);
        auto* load = b.LoadVectorElement(access_2, 2_u);
        auto* mul = b.Multiply<vec4<f32>>(b.Splat<vec4<f32>>(1_f, 4), load);
        b.Store(access_2, mul);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %color:ptr<__out, vec4<f32>, read_write> = var @location(1)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    %3:ptr<__out, vec4<f32>, read_write> = access %color
    %4:ptr<__out, vec4<f32>, read_write> = access %3
    %5:f32 = load_vector_element %4, 2u
    %6:vec4<f32> = mul vec4<f32>(1.0f), %5
    store %4, %6
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %color:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func():void -> %b2 {
  %b2 = block {
    %3:ptr<private, vec4<f32>, read_write> = access %color
    %4:ptr<private, vec4<f32>, read_write> = access %3
    %5:f32 = load_vector_element %4, 2u
    %6:vec4<f32> = mul vec4<f32>(1.0f), %5
    store %4, %6
    ret
  }
}
%foo = @fragment func():vec4<f32> [@location(1)] -> %b3 {
  %b3 = block {
    %8:void = call %foo_inner
    %9:vec4<f32> = load %color
    ret %9
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

TEST_F(SpirvReader_ShaderIOTest, Inputs_And_Outputs) {
    auto* position = b.Var("position", ty.ptr(core::AddressSpace::kIn, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.builtin = core::BuiltinValue::kPosition;
        attributes.invariant = true;
        position->SetAttributes(std::move(attributes));
    }
    auto* color_in = b.Var("color_in", ty.ptr(core::AddressSpace::kIn, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 0;
        color_in->SetAttributes(std::move(attributes));
    }
    auto* color_out_1 = b.Var("color_out_1", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 1;
        color_out_1->SetAttributes(std::move(attributes));
    }
    auto* color_out_2 = b.Var("color_out_2", ty.ptr(core::AddressSpace::kOut, ty.vec4<f32>()));
    {
        core::ir::IOAttributes attributes;
        attributes.location = 2;
        color_out_2->SetAttributes(std::move(attributes));
    }
    mod.root_block->Append(position);
    mod.root_block->Append(color_in);
    mod.root_block->Append(color_out_1);
    mod.root_block->Append(color_out_2);

    auto* ep = b.Function("foo", ty.void_(), core::ir::Function::PipelineStage::kFragment);
    b.Append(ep->Block(), [&] {
        auto* position_value = b.Load(position);
        auto* color_in_value = b.Load(color_in);
        b.Store(color_out_1, position_value);
        b.Store(color_out_2, color_in_value);
        b.Return(ep);
    });

    auto* src = R"(
%b1 = block {  # root
  %position:ptr<__in, vec4<f32>, read_write> = var @invariant @builtin(position)
  %color_in:ptr<__in, vec4<f32>, read_write> = var @location(0)
  %color_out_1:ptr<__out, vec4<f32>, read_write> = var @location(1)
  %color_out_2:ptr<__out, vec4<f32>, read_write> = var @location(2)
}

%foo = @fragment func():void -> %b2 {
  %b2 = block {
    %6:vec4<f32> = load %position
    %7:vec4<f32> = load %color_in
    store %color_out_1, %6
    store %color_out_2, %7
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
tint_symbol = struct @align(16) {
  color_out_1:vec4<f32> @offset(0), @location(1)
  color_out_2:vec4<f32> @offset(16), @location(2)
}

%b1 = block {  # root
  %color_out_1:ptr<private, vec4<f32>, read_write> = var
  %color_out_2:ptr<private, vec4<f32>, read_write> = var
}

%foo_inner = func(%position:vec4<f32>, %color_in:vec4<f32>):void -> %b2 {
  %b2 = block {
    store %color_out_1, %position
    store %color_out_2, %color_in
    ret
  }
}
%foo = @fragment func(%position_1:vec4<f32> [@invariant, @position], %color_in_1:vec4<f32> [@location(0)]):tint_symbol -> %b3 {  # %position_1: 'position', %color_in_1: 'color_in'
  %b3 = block {
    %9:void = call %foo_inner, %position_1, %color_in_1
    %10:vec4<f32> = load %color_out_1
    %11:vec4<f32> = load %color_out_2
    %12:tint_symbol = construct %10, %11
    ret %12
  }
}
)";

    Run(ShaderIO);

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::spirv::reader::lower
