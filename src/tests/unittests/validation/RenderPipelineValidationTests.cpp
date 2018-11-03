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

            dawn::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, nullptr);

            inputState = device.CreateInputStateBuilder().GetResult();

            blendState = device.CreateBlendStateBuilder().GetResult();

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
        dawn::InputState inputState;
        dawn::BlendState blendState;
        dawn::PipelineLayout pipelineLayout;
};

// Test cases where creation should succeed
TEST_F(RenderPipelineValidationTest, CreationSuccess) {
    uint32_t colorAttachments[] = {0};
    dawn::TextureFormat colorAttachmentFormats[] =
        {dawn::TextureFormat::R8G8B8A8Unorm};

    dawn::ShaderStage renderStages[] = {dawn::ShaderStage::Vertex, dawn::ShaderStage::Fragment};
    dawn::ShaderModule renderModules[] = {vsModule, fsModule};

    utils::ComboRenderPipelineDescriptor descriptor;
    descriptor.layout = pipelineLayout;
    descriptor.numOfRenderStages = 2;
    descriptor.stages = renderStages;
    descriptor.modules = renderModules;
    descriptor.entryPoint = "main";
    descriptor.numOfColorAttachments = 1;
    descriptor.colorAttachments = colorAttachments;
    descriptor.colorAttachmentFormats = colorAttachmentFormats;
    dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
    descriptor.blendStates = blendStates;

    descriptor.SetDefaults(device);

    device.CreateRenderPipeline(&descriptor);

    descriptor.layout = pipelineLayout;
    descriptor.numOfRenderStages = 2;
    descriptor.stages = renderStages;
    descriptor.modules = renderModules;
    descriptor.entryPoint = "main";
    descriptor.numOfColorAttachments = 1;
    descriptor.inputState = inputState;
    descriptor.colorAttachments = colorAttachments;
    descriptor.colorAttachmentFormats = colorAttachmentFormats;
    descriptor.blendStates = blendStates;

    descriptor.SetDefaults(device);

    device.CreateRenderPipeline(&descriptor);

    descriptor.layout = pipelineLayout;
    descriptor.numOfRenderStages = 2;
    descriptor.stages = renderStages;
    descriptor.modules = renderModules;
    descriptor.entryPoint = "main";
    descriptor.numOfColorAttachments = 1;
    descriptor.inputState = inputState;
    descriptor.colorAttachments = colorAttachments;
    descriptor.colorAttachmentFormats = colorAttachmentFormats;
    descriptor.blendStates = blendStates;

    descriptor.SetDefaults(device);

    device.CreateRenderPipeline(&descriptor);
}

// Test creation failure when properties are missing
TEST_F(RenderPipelineValidationTest, CreationMissingProperty) {
    uint32_t colorAttachments[] = {0};
    dawn::TextureFormat colorAttachmentFormats[] =
        {dawn::TextureFormat::R8G8B8A8Unorm};

    dawn::ShaderStage renderStages[] = {dawn::ShaderStage::Vertex, dawn::ShaderStage::Fragment};
    dawn::ShaderModule renderModules[] = {vsModule, fsModule};

    utils::ComboRenderPipelineDescriptor descriptor;
    // Vertex stage not set
    {
        dawn::ShaderStage wrongRenderStages[] = {dawn::ShaderStage::Fragment};
        dawn::ShaderModule wrongRenderModules[] = {fsModule};

        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = wrongRenderStages;
        descriptor.modules = wrongRenderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.colorAttachmentFormats = colorAttachmentFormats;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        descriptor.SetDefaults(device);

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Fragment stage not set
    {
        dawn::ShaderStage wrongRenderStages[] = {dawn::ShaderStage::Vertex};
        dawn::ShaderModule wrongRenderModules[] = {vsModule};

        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = wrongRenderStages;
        descriptor.modules = wrongRenderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.colorAttachmentFormats = colorAttachmentFormats;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        descriptor.SetDefaults(device);

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // No attachment set
    {
        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1; // the fake number.
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        descriptor.SetDefaults(device);

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}

TEST_F(RenderPipelineValidationTest, BlendState) {
    // Fails because blend state is set on a nonexistent color attachment

    uint32_t colorAttachments[] = {0};
    dawn::TextureFormat colorAttachmentFormats[] =
        {dawn::TextureFormat::R8G8B8A8Unorm};

    dawn::ShaderStage renderStages[] = {dawn::ShaderStage::Vertex, dawn::ShaderStage::Fragment};
    dawn::ShaderModule renderModules[] = {vsModule, fsModule};

    utils::ComboRenderPipelineDescriptor descriptor;
    {
        // This one succeeds because attachment 0 is the color attachment
        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.colorAttachmentFormats = colorAttachmentFormats;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        descriptor.SetDefaults(device);

        device.CreateRenderPipeline(&descriptor);

        // Fail because lack of blend states for color attachments
        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.colorAttachmentFormats = colorAttachmentFormats;

        descriptor.SetDefaults(device);

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}

// TODO(enga@google.com): These should be added to the test above when validation is implemented
TEST_F(RenderPipelineValidationTest, DISABLED_TodoCreationMissingProperty) {
    uint32_t colorAttachments[] = {0};
    dawn::TextureFormat colorAttachmentFormats[] =
        {dawn::TextureFormat::R8G8B8A8Unorm};

    dawn::ShaderStage renderStages[] = {dawn::ShaderStage::Vertex, dawn::ShaderStage::Fragment};
    dawn::ShaderModule renderModules[] = {vsModule, fsModule};

    // Fails because pipeline layout is not set
    {
        dawn::RenderPipelineDescriptor descriptor;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor.indexFormat = dawn::IndexFormat::Uint32;
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.colorAttachmentFormats = colorAttachmentFormats;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));

    }

    // Fails because primitive topology is not set
    {
        dawn::RenderPipelineDescriptor descriptor;
        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor.indexFormat = dawn::IndexFormat::Uint32;
        descriptor.colorAttachmentFormats = colorAttachmentFormats;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Fails because index format is not set
    {
        dawn::RenderPipelineDescriptor descriptor;
        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.numOfColorAttachments = 1;
        descriptor.colorAttachments = colorAttachments;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        descriptor.colorAttachmentFormats = colorAttachmentFormats;
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }

    // Fails because num of color attachment is zero
    {
        dawn::RenderPipelineDescriptor descriptor;
        descriptor.layout = pipelineLayout;
        descriptor.numOfRenderStages = 2;
        descriptor.stages = renderStages;
        descriptor.modules = renderModules;
        descriptor.entryPoint = "main";
        descriptor.primitiveTopology = dawn::PrimitiveTopology::TriangleList;
        descriptor.indexFormat = dawn::IndexFormat::Uint32;
        descriptor.numOfColorAttachments = 0;
        descriptor.inputState = device.CreateInputStateBuilder().GetResult();
        descriptor.depthStencilState = device.CreateDepthStencilStateBuilder().GetResult();
        dawn::BlendState blendStates[] = {device.CreateBlendStateBuilder().GetResult()};
        descriptor.blendStates = blendStates;

        ASSERT_DEVICE_ERROR(device.CreateRenderPipeline(&descriptor));
    }
}
