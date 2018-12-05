// Copyright 2017 The Dawn Authors
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

#include "common/Constants.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

class RenderPipelineValidationTest : public ValidationTest {
    protected:
        void SetUp() override {
            ValidationTest::SetUp();

            renderpass = CreateSimpleRenderPass();

            vsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
                #version 450
                void main() {
                    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
                })"
            );

            fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
                #version 450
                layout(location = 0) out vec4 fragColor;
                void main() {
                    fragColor = vec4(0.0, 1.0, 0.0, 1.0);
                })");
        }

        dawn::RenderPassDescriptor renderpass;
        dawn::ShaderModule vsModule;
        dawn::ShaderModule fsModule;
};

// Test cases where creation should succeed
TEST_F(RenderPipelineValidationTest, CreationSuccess) {
    utils::ComboRenderPipelineDescriptor descriptor(&device);
    descriptor.vertexStage.module = vsModule;
    descriptor.fragmentStage.module = fsModule;

    device.CreateRenderPipeline(&descriptor);
}

// Test creation failure when properties are missing
TEST_F(RenderPipelineValidationTest, CreationMissingProperty) {

    // Vertex stage not set
    {
        utils::ComboRenderPipelineDescriptor descriptor(&device);
        descriptor.fragmentStage.module = fsModule;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Fragment stage not set
    {
        utils::ComboRenderPipelineDescriptor descriptor(&device);
        descriptor.vertexStage.module = vsModule;
       
        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // No attachment set
    {
        utils::ComboRenderPipelineDescriptor descriptor(&device);
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;
        descriptor.renderAttachmentsState.numColorAttachments = 0;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}

TEST_F(RenderPipelineValidationTest, BlendState) {

    {
        // This one succeeds because attachment 0 is the color attachment
        utils::ComboRenderPipelineDescriptor descriptor(&device);
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;

        device.CreateRenderPipeline(&descriptor);
    }

    {   // Fail because lack of blend states for color attachments
        utils::ComboRenderPipelineDescriptor descriptor(&device);
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;
        descriptor.numBlendStates = 0;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    {
        // Fail because set blend states for empty color attachments
        utils::ComboRenderPipelineDescriptor descriptor(&device);
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;
        descriptor.numBlendStates = 2;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}

// TODO(enga@google.com): These should be added to the test above when validation is implemented
TEST_F(RenderPipelineValidationTest, DISABLED_TodoCreationMissingProperty) {
    // Fails because pipeline layout is not set
    {
        dawn::RenderPipelineDescriptor descriptor;
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;
        descriptor.vertexStage.entryPoint = "main";
        descriptor.fragmentStage.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.renderAttachmentsState.numColorAttachments = 1;
        descriptor.renderAttachmentsState.colorAttachments[0].format =
            dawn::TextureFormat::R8G8B8A8Unorm;
        descriptor.renderAttachmentsState.colorAttachments[0].samples = 1;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor.indexFormat = dawn::IndexFormat::Uint32;
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numBlendStates = 1;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));

    }

    // Fails because primitive topology is not set
    {
        dawn::RenderPipelineDescriptor descriptor;
        
        dawn::PipelineLayoutDescriptor layoutDescriptor;
        layoutDescriptor.numBindGroupLayouts = 0;
        layoutDescriptor.bindGroupLayouts = nullptr;

        descriptor.layout = device.CreatePipelineLayout(&layoutDescriptor);
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;
        descriptor.vertexStage.entryPoint = "main";
        descriptor.fragmentStage.entryPoint = "main";
        descriptor.renderAttachmentsState.numColorAttachments = 1;
        descriptor.renderAttachmentsState.colorAttachments[0].format =
            dawn::TextureFormat::R8G8B8A8Unorm;
        descriptor.renderAttachmentsState.colorAttachments[0].samples = 1;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor.indexFormat = dawn::IndexFormat::Uint32;
        descriptor.numBlendStates = 1;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Fails because index format is not set
    {
        dawn::RenderPipelineDescriptor descriptor;
        
        dawn::PipelineLayoutDescriptor layoutDescriptor;
        layoutDescriptor.numBindGroupLayouts = 0;
        layoutDescriptor.bindGroupLayouts = nullptr;

        descriptor.layout = device.CreatePipelineLayout(&layoutDescriptor);
        descriptor.vertexStage.module = vsModule;
        descriptor.fragmentStage.module = fsModule;
        descriptor.vertexStage.entryPoint = "main";
        descriptor.fragmentStage.entryPoint = "main";
        descriptor.renderAttachmentsState.numColorAttachments = 1;
        descriptor.renderAttachmentsState.colorAttachments[0].format =
            dawn::TextureFormat::R8G8B8A8Unorm;
        descriptor.renderAttachmentsState.colorAttachments[0].samples = 1;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numBlendStates = 1;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}
