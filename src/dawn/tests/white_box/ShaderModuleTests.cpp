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
#include <memory>
#include <string>
#include <vector>

#include "dawn/native/ShaderModule.h"
#include "dawn/tests/DawnTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn::native {
namespace {

struct CreatePipelineAsyncTask {
    wgpu::ComputePipeline computePipeline = nullptr;
    wgpu::RenderPipeline renderPipeline = nullptr;
    bool isCompleted = false;
    std::string message;
};

class ShaderModuleTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();

        DAWN_TEST_UNSUPPORTED_IF(UsesWire());
    }

    bool SupportsCreatePipelineAsync() const {
        // OpenGL and OpenGLES don't support it.
        if (IsOpenGL() || IsOpenGLES()) {
            return false;
        }

        // Async pipeline creation is disabled with Metal AMD and Validation.
        // See crbug.com/dawn/1200.
        if (IsMetal() && IsAMD() && IsMetalValidationEnabled()) {
            return false;
        }

        return true;
    }

    void DoCreateRenderPipelineAsync(
        const utils::ComboRenderPipelineDescriptor& renderPipelineDescriptor) {
        device.CreateRenderPipelineAsync(
            &renderPipelineDescriptor,
            [](WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline returnPipeline,
               const char* message, void* userdata) {
                EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_Success,
                          status);

                CreatePipelineAsyncTask* currentTask =
                    static_cast<CreatePipelineAsyncTask*>(userdata);
                currentTask->renderPipeline = wgpu::RenderPipeline::Acquire(returnPipeline);
                currentTask->isCompleted = true;
                currentTask->message = message;
            },
            &task);
    }

    CreatePipelineAsyncTask task;
};

TEST_P(ShaderModuleTests, CachedShader) {
    const char* kVertexShader = R"(
        @vertex fn main(
            @builtin(vertex_index) VertexIndex : u32
        ) -> @builtin(position) vec4f {
            var pos = array(
                vec2f( 0.0,  0.5),
                vec2f(-0.5, -0.5),
                vec2f( 0.5, -0.5)
            );
            return vec4f(pos[VertexIndex], 0.0, 1.0);
        })";

    wgpu::ShaderModule module = utils::CreateShaderModule(device, kVertexShader);

    // Add a internal reference
    Ref<ShaderModuleBase> shaderModule(FromAPI(module.Get()));
    EXPECT_TRUE(shaderModule.Get());
    EXPECT_EQ(shaderModule->GetRefCountForTesting(), 2ull);
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 1ull);

    EXPECT_TRUE(shaderModule->GetTintProgram());

    auto scopedUseTintProgram = shaderModule->UseTintProgram();

    // UseTintProgram() should increase the external ref count
    EXPECT_TRUE(shaderModule->GetTintProgramForTesting());
    EXPECT_FALSE(shaderModule->GetTintProgramIsReCreatedForTesting());
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 2ull);

    // Drop the external reference
    module = {};

    // The mTintProgram should be reset.
    EXPECT_TRUE(shaderModule->GetTintProgramForTesting());
    EXPECT_FALSE(shaderModule->GetTintProgramIsReCreatedForTesting());
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 1ull);

    // Drop the scopedUseTintProgram
    scopedUseTintProgram = nullptr;
    EXPECT_FALSE(shaderModule->GetTintProgramForTesting());
    EXPECT_FALSE(shaderModule->GetTintProgramIsReCreatedForTesting());
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 0ull);

    // Create a ShaderModule with the same source code.
    module = utils::CreateShaderModule(device, kVertexShader);

    // The module should be from the cache.
    EXPECT_EQ(shaderModule.Get(), FromAPI(module.Get()));

    // The mTintProgram should be null.
    EXPECT_FALSE(shaderModule->GetTintProgramForTesting());
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 1ull);
    EXPECT_FALSE(shaderModule->GetTintProgramIsReCreatedForTesting());

    // Call UseTintProgram() should re-create mTintProgram.
    scopedUseTintProgram = shaderModule->UseTintProgram();
    EXPECT_TRUE(shaderModule->GetTintProgramForTesting());
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 2ull);
    EXPECT_TRUE(shaderModule->GetTintProgramIsReCreatedForTesting());
}

TEST_P(ShaderModuleTests, CachedShaderAndRenderPipeline) {
    constexpr wgpu::TextureFormat kRenderAttachmentFormat = wgpu::TextureFormat::RGBA8Unorm;

    const char* kVertexShader = R"(
        @vertex fn main(
            @builtin(vertex_index) VertexIndex : u32
        ) -> @builtin(position) vec4f {
            var pos = array(
                vec2f( 0.0,  0.5),
                vec2f(-0.5, -0.5),
                vec2f( 0.5, -0.5)
            );
            return vec4f(pos[VertexIndex], 0.0, 1.0);
        })";

    const char* kFragmentShader = R"(
        @fragment fn main() -> @location(0) vec4f {
            return vec4f(0.0, 1.0, 0.0, 1.0);
        })";

    wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, kVertexShader);
    wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, kFragmentShader);

    {
        utils::ComboRenderPipelineDescriptor renderPipelineDescriptor;
        renderPipelineDescriptor.vertex.module = vsModule;
        renderPipelineDescriptor.cFragment.module = fsModule;
        renderPipelineDescriptor.cTargets[0].format = kRenderAttachmentFormat;
        renderPipelineDescriptor.primitive.topology = wgpu::PrimitiveTopology::PointList;

        std::string vertexEntryPoint = "main";
        std::string fragmentEntryPoint = "main";
        renderPipelineDescriptor.vertex.entryPoint = vertexEntryPoint.c_str();
        renderPipelineDescriptor.cFragment.entryPoint = fragmentEntryPoint.c_str();

        DoCreateRenderPipelineAsync(renderPipelineDescriptor);
    }

    Ref<ShaderModuleBase> vsShaderModule(FromAPI(vsModule.Get()));
    EXPECT_TRUE(vsShaderModule.Get());
    EXPECT_TRUE(vsShaderModule->GetTintProgramForTesting());

    Ref<ShaderModuleBase> fsShaderModule(FromAPI(fsModule.Get()));
    EXPECT_TRUE(fsShaderModule.Get());
    EXPECT_TRUE(fsShaderModule->GetTintProgramForTesting());

    if (!SupportsCreatePipelineAsync()) {
        EXPECT_EQ(vsShaderModule->GetExternalRefCountForTesting(), 1ull);
        EXPECT_EQ(fsShaderModule->GetExternalRefCountForTesting(), 1ull);
        do {
            WaitABit();
        } while (!task.isCompleted);

        EXPECT_TRUE(task.renderPipeline);
        return;
    }

    // If async pipeline creation is supported.
    EXPECT_EQ(vsShaderModule->GetExternalRefCountForTesting(), 2ull);
    EXPECT_EQ(fsShaderModule->GetExternalRefCountForTesting(), 2ull);

    // Drop the external reference.
    vsModule = {};
    EXPECT_EQ(vsShaderModule->GetExternalRefCountForTesting(), 1ull);
    EXPECT_TRUE(vsShaderModule->GetTintProgramForTesting());

    // Drop the external reference.
    fsModule = {};
    EXPECT_EQ(fsShaderModule->GetExternalRefCountForTesting(), 1ull);
    EXPECT_TRUE(fsShaderModule->GetTintProgramForTesting());

    // Wait until pipeline creation is done.
    do {
        WaitABit();
    } while (!task.isCompleted);

    EXPECT_TRUE(task.renderPipeline);

    // When the pipeline compilation is done, the external refcount should be 0.
    EXPECT_EQ(vsShaderModule->GetExternalRefCountForTesting(), 0ull);
    EXPECT_EQ(fsShaderModule->GetExternalRefCountForTesting(), 0ull);

    // mTintProgram should be released already.
    EXPECT_FALSE(vsShaderModule->GetTintProgramForTesting());
    EXPECT_FALSE(fsShaderModule->GetTintProgramForTesting());
}

TEST_P(ShaderModuleTests, CachedShaderAndComputePipeline) {
    wgpu::ShaderModule module = utils::CreateShaderModule(device, R"(
        struct SSBO {
            value : u32
        }
        @group(0) @binding(0) var<storage, read_write> ssbo : SSBO;

        @compute @workgroup_size(1) fn main() {
            ssbo.value = 1u;
        })");

    {
        wgpu::ComputePipelineDescriptor csDesc;
        csDesc.compute.module = module;
        csDesc.compute.entryPoint = "main";

        device.CreateComputePipelineAsync(
            &csDesc,
            [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline returnPipeline,
               const char* message, void* userdata) {
                EXPECT_EQ(WGPUCreatePipelineAsyncStatus::WGPUCreatePipelineAsyncStatus_Success,
                          status);

                CreatePipelineAsyncTask* task = static_cast<CreatePipelineAsyncTask*>(userdata);
                task->computePipeline = wgpu::ComputePipeline::Acquire(returnPipeline);
                task->isCompleted = true;
                task->message = message;
            },
            &task);
    }

    Ref<ShaderModuleBase> shaderModule(FromAPI(module.Get()));
    EXPECT_TRUE(shaderModule.Get());
    EXPECT_TRUE(shaderModule->GetTintProgramForTesting());

    if (SupportsCreatePipelineAsync()) {
        EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 2ull);
    } else {
        EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 1ull);
        do {
            WaitABit();
        } while (!task.isCompleted);

        EXPECT_TRUE(task.computePipeline);
        return;
    }

    // If async pipeline creation is supported.
    // Drop the external reference.
    module = {};
    EXPECT_TRUE(shaderModule->GetTintProgramForTesting());
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 1ull);

    do {
        WaitABit();
    } while (!task.isCompleted);

    EXPECT_TRUE(task.computePipeline);

    // When the pipeline compilation is done, the tintData refcount should be 0.
    EXPECT_EQ(shaderModule->GetExternalRefCountForTesting(), 0ull);

    // mTintProgram should be released already.
    EXPECT_FALSE(shaderModule->GetTintProgramForTesting());
}

DAWN_INSTANTIATE_TEST(ShaderModuleTests,
                      D3D11Backend(),
                      D3D12Backend(),
                      MetalBackend(),
                      VulkanBackend(),
                      OpenGLBackend(),
                      OpenGLESBackend());

}  // anonymous namespace
}  // namespace dawn::native
