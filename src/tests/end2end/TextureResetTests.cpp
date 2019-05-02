// Copyright 2019 The Dawn Authors
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

#include "tests/DawnTest.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"

class TextureResetTest : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
    }
    dawn::RenderPipeline CreateQuadPipeline(dawn::TextureFormat format) {
        dawn::ShaderModule vsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
            #version 450
            const vec2 pos[3] = vec2[](
                vec2(-1.0f, -1.0f), vec2(-1.0f, 1.0f), vec2(1.0f, -1.0f)
            );
            void main() {
                gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
            })");

        dawn::ShaderModule fsModule =
            utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
            #version 450
            layout(location = 0) out vec4 fragColor;
            void main() {
                fragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
            })");

        utils::ComboRenderPipelineDescriptor descriptor(device);
        descriptor.cVertexStage.module = vsModule;
        descriptor.cFragmentStage.module = fsModule;
        descriptor.cColorStates[0]->format = format;

        return device.CreateRenderPipeline(&descriptor);
    }
};

// Test that texture is dirty after destroy
TEST_P(TextureResetTest, RecycleTextureMemoryClear) {
    int numTextures = 50;
    RGBA8 filled(0, 255, 0, 255);
    RGBA8 not_filled(0, 0, 0, 0);
    utils::BasicRenderPass renderPass;
    dawn::Texture text_arr[numTextures];
    dawn::RenderPipeline pipeline = CreateQuadPipeline(renderPass.colorFormat);

    // Create a lot of textures and draw to every other texture
    for (int i = 0; i < numTextures; i++) {
        renderPass = utils::CreateBasicRenderPass(device, 128, 128);
        text_arr[i] = renderPass.color;

        // new texture should not be filled
        EXPECT_PIXEL_RGBA8_EQ(not_filled, renderPass.color, 0, 0);
        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(pipeline);
            if (i % 2 == 0) {
                pass.Draw(3, 1, 0, 0);
            }
            pass.EndPass();
        }
        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        if (i % 2 == 0) {
            EXPECT_PIXEL_RGBA8_EQ(filled, renderPass.color, 0, 0);
        } else {
            EXPECT_PIXEL_RGBA8_EQ(not_filled, renderPass.color, 0, 0);
        }
    }

    // Go through and destroy all the textures
    for (int i = 0; i < numTextures; i++) {
        text_arr[i].Destroy();
    }

    // Create textures again to see although recycled memory, the textures clear
    for (int i = 0; i < numTextures; i++) {
        renderPass = utils::CreateBasicRenderPass(device, 128, 128);

        // expect new texture is clear
        EXPECT_PIXEL_RGBA8_EQ(not_filled, renderPass.color, 0, 0);

        dawn::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass.renderPassInfo);
            pass.SetPipeline(pipeline);
            pass.EndPass();
        }
        dawn::CommandBuffer commands = encoder.Finish();
        queue.Submit(1, &commands);

        // nothing was drawn, texture should still be unfilled
        EXPECT_PIXEL_RGBA8_EQ(not_filled, renderPass.color, 0, 0);
    }
}

DAWN_INSTANTIATE_TEST(TextureResetTest, VulkanBackend);