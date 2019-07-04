// Copyright 2018 The Dawn Authors
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

#include "tests/unittests/validation/ValidationTest.h"

#include <cmath>

class SetScissorRectTest : public ValidationTest {
};

// Test to check basic use of SetScissor
TEST_F(SetScissorRectTest, Success) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(0, 0, 1, 1);
        pass.EndPass();
    }
    encoder.Finish();
}

// Test to check that any scissor rect parameter is NaN is not allowed
TEST_F(SetScissorRectTest, ScissorRectIsNaN) {
    DummyRenderPass renderPass(device);

    // x or y is NAN.
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(NAN, 0, 1, 1);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // width or height is NAN.
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(0, 0, NAN, 1);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test to check that an empty scissor is not allowed
TEST_F(SetScissorRectTest, EmptyScissor) {
    DummyRenderPass renderPass(device);

    // Width of scissor rect is zero.
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(0, 0, 0, 1);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Height of scissor rect is zero.
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(0, 0, 1, 0);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    // Both width and height of scissor rect are zero.
    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(0, 0, 0, 0);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test to check that a scissor larger than the framebuffer is allowed
TEST_F(SetScissorRectTest, ScissorLargerThanFramebuffer) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetScissorRect(0, 0, renderPass.width + 1, renderPass.height + 1);
        pass.EndPass();
    }
    encoder.Finish();
}

class SetBlendColorTest : public ValidationTest {
};

// Test to check basic use of SetBlendColor
TEST_F(SetBlendColorTest, Success) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        constexpr dawn::Color kTransparentBlack{0.0f, 0.0f, 0.0f, 0.0f};
        pass.SetBlendColor(&kTransparentBlack);
        pass.EndPass();
    }
    encoder.Finish();
}

// Test that color or alpha is NaN is not allowed
TEST_F(SetBlendColorTest, ColorOrAlphaIsNaN) {
    DummyRenderPass renderPass(device);

    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        constexpr dawn::Color kNaNColor{NAN, 0.0f, 0.0f, 0.0f};
        pass.SetBlendColor(&kNaNColor);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }

    {
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        constexpr dawn::Color kNaNAlpha{0.0f, 0.0f, 0.0f, NAN};
        pass.SetBlendColor(&kNaNAlpha);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test that SetBlendColor allows any value, large, small or negative
TEST_F(SetBlendColorTest, AnyValueAllowed) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        constexpr dawn::Color kAnyColorValue{-1.0f, 42.0f, -0.0f, 0.0f};
        pass.SetBlendColor(&kAnyColorValue);
        pass.EndPass();
    }
    encoder.Finish();
}

class SetStencilReferenceTest : public ValidationTest {
};

// Test to check basic use of SetStencilReferenceTest
TEST_F(SetStencilReferenceTest, Success) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetStencilReference(0);
        pass.EndPass();
    }
    encoder.Finish();
}

// Test that reference is NaN is not allowed
TEST_F(SetStencilReferenceTest, ReferenceIsNaN) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetStencilReference(NAN);
        pass.EndPass();
        ASSERT_DEVICE_ERROR(encoder.Finish());
    }
}

// Test that SetStencilReference allows any bit to be set
TEST_F(SetStencilReferenceTest, AllBitsAllowed) {
    DummyRenderPass renderPass(device);

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetStencilReference(0xFFFFFFFF);
        pass.EndPass();
    }
    encoder.Finish();
}
