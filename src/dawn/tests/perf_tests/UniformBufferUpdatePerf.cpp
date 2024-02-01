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

#include <queue>
#include <vector>

#include "dawn/common/MutexProtected.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

namespace dawn {
namespace {

constexpr unsigned int kNumIterations = 100;

constexpr uint32_t kTextureSize = 128;
constexpr size_t kUniformDataSize = 3 * sizeof(float);
constexpr size_t kUniformBufferSize = 256;

constexpr float kVertexData[12] = {
    0.0f, 0.5f, 0.0f, 1.0f, -0.5f, -0.5f, 0.0f, 1.0f, 0.5f, -0.5f, 0.0f, 1.0f,
};

constexpr char kVertexShader[] = R"(
        @vertex fn main(
            @location(0) pos : vec4f
        ) -> @builtin(position) vec4f {
            return pos;
        })";

constexpr char kFragmentShader[] = R"(
        @group(0) @binding(0) var<uniform> color : vec3f;
        @fragment fn main() -> @location(0) vec4f {
            return vec4f(color * (1.0 / 5000.0), 1.0);
        })";

enum class UploadMethod {
    WriteBuffer,
    StagingBuffer,
};

enum class UploadSize {
    Partial,
    Full,
};

struct UniformBufferUpdateParams : AdapterTestParam {
    UniformBufferUpdateParams(const AdapterTestParam& param,
                              UploadMethod uploadMethod,
                              UploadSize uploadSize)
        : AdapterTestParam(param), uploadMethod(uploadMethod), uploadSize(uploadSize) {}

    UploadMethod uploadMethod;
    UploadSize uploadSize;
};

std::ostream& operator<<(std::ostream& ostream, const UniformBufferUpdateParams& param) {
    ostream << static_cast<const AdapterTestParam&>(param);

    switch (param.uploadMethod) {
        case UploadMethod::WriteBuffer:
            ostream << "_WriteBuffer";
            break;
        case UploadMethod::StagingBuffer:
            ostream << "_StagingBuffer";
            break;
    }

    switch (param.uploadSize) {
        case UploadSize::Partial:
            ostream << "_Partial";
            break;
        case UploadSize::Full:
            ostream << "_Full";
            break;
    }

    return ostream;
}

// Test updating a uniform buffer |kNumIterations| times.
class UniformBufferUpdatePerf : public DawnPerfTestWithParams<UniformBufferUpdateParams> {
  public:
    UniformBufferUpdatePerf() : DawnPerfTestWithParams(kNumIterations, 1) {}
    ~UniformBufferUpdatePerf() override = default;

    void SetUp() override;

  private:
    struct CallbackData {
        UniformBufferUpdatePerf* self;
        wgpu::Buffer buffer;
    };
    void Step() override;

    size_t GetBufferSize();
    wgpu::Buffer FindOrCreateUniformBuffer();
    void ReturnUniformBuffer(wgpu::Buffer buffer);
    wgpu::Buffer FindOrCreateStagingBuffer();
    void ReturnStagingBuffer(wgpu::Buffer buffer);

    wgpu::TextureView mColorAttachment;
    wgpu::TextureView mDepthStencilAttachment;
    wgpu::Buffer mVertexBuffer;
    wgpu::BindGroupLayout mUniformBindGroupLayout;
    wgpu::RenderPipeline mPipeline;

    MutexProtected<std::queue<wgpu::Buffer>> mUniformBuffers;
    MutexProtected<std::queue<wgpu::Buffer>> mStagingBuffers;
};

size_t UniformBufferUpdatePerf::GetBufferSize() {
    return GetParam().uploadSize == UploadSize::Full ? kUniformDataSize : kUniformBufferSize;
}
wgpu::Buffer UniformBufferUpdatePerf::FindOrCreateUniformBuffer() {
    if (!mUniformBuffers->empty()) {
        wgpu::Buffer buffer = mUniformBuffers->front();
        mUniformBuffers->pop();
        return buffer;
    }
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    descriptor.size = GetBufferSize();
    return device.CreateBuffer(&descriptor);
}

void UniformBufferUpdatePerf::ReturnUniformBuffer(wgpu::Buffer buffer) {
    mUniformBuffers->push(buffer);
}

wgpu::Buffer UniformBufferUpdatePerf::FindOrCreateStagingBuffer() {
    if (!mStagingBuffers->empty()) {
        wgpu::Buffer buffer = mStagingBuffers->front();
        mStagingBuffers->pop();
        return buffer;
    }
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
    descriptor.size = GetBufferSize();
    descriptor.mappedAtCreation = true;
    return device.CreateBuffer(&descriptor);
}

void UniformBufferUpdatePerf::ReturnStagingBuffer(wgpu::Buffer buffer) {
    mStagingBuffers->push(buffer);
}

void UniformBufferUpdatePerf::SetUp() {
    DawnPerfTestWithParams<UniformBufferUpdateParams>::SetUp();

    // Create the color / depth stencil attachments.
    wgpu::TextureDescriptor descriptor = {};
    descriptor.dimension = wgpu::TextureDimension::e2D;
    descriptor.size.width = kTextureSize;
    descriptor.size.height = kTextureSize;
    descriptor.size.depthOrArrayLayers = 1;
    descriptor.usage = wgpu::TextureUsage::RenderAttachment;

    descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    mColorAttachment = device.CreateTexture(&descriptor).CreateView();

    descriptor.format = wgpu::TextureFormat::Depth24PlusStencil8;
    mDepthStencilAttachment = device.CreateTexture(&descriptor).CreateView();

    // Create the vertex buffer
    mVertexBuffer = utils::CreateBufferFromData(device, kVertexData, sizeof(kVertexData),
                                                wgpu::BufferUsage::Vertex);

    // Create the bind group layout.
    mUniformBindGroupLayout = utils::MakeBindGroupLayout(
        device, {
                    {0, wgpu::ShaderStage::Fragment, wgpu::BufferBindingType::Uniform, false},
                });

    // Setup the base render pipeline descriptor.
    utils::ComboRenderPipelineDescriptor renderPipelineDesc;
    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.cBuffers[0].arrayStride = 4 * sizeof(float);
    renderPipelineDesc.cBuffers[0].attributeCount = 1;
    renderPipelineDesc.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
    renderPipelineDesc.EnableDepthStencil(wgpu::TextureFormat::Depth24PlusStencil8);
    renderPipelineDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;

    // Create the pipeline layout for the pipeline.
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayouts = &mUniformBindGroupLayout;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

    // Create the shaders for the pipeline.
    wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, kVertexShader);
    wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, kFragmentShader);

    // Create the pipeline.
    renderPipelineDesc.layout = pipelineLayout;
    renderPipelineDesc.vertex.module = vsModule;
    renderPipelineDesc.cFragment.module = fsModule;
    mPipeline = device.CreateRenderPipeline(&renderPipelineDesc);
}

void UniformBufferUpdatePerf::Step() {
    for (unsigned int i = 0; i < kNumIterations; ++i) {
        std::vector<float> data(kUniformDataSize, 1.0f * i);
        wgpu::CommandEncoder commands = device.CreateCommandEncoder();
        wgpu::Buffer uniformBuffer = FindOrCreateUniformBuffer();
        wgpu::Buffer stagingBuffer = nullptr;
        switch (GetParam().uploadMethod) {
            case UploadMethod::WriteBuffer:
                queue.WriteBuffer(uniformBuffer, 0, data.data(), data.size());
                break;
            case UploadMethod::StagingBuffer:
                stagingBuffer = FindOrCreateStagingBuffer();
                memcpy(stagingBuffer.GetMappedRange(0, data.size()), data.data(), data.size());
                stagingBuffer.Unmap();
                commands.CopyBufferToBuffer(stagingBuffer, 0, uniformBuffer, 0, data.size());
                break;
        }
        utils::ComboRenderPassDescriptor renderPass({mColorAttachment}, mDepthStencilAttachment);
        wgpu::RenderPassEncoder pass = commands.BeginRenderPass(&renderPass);
        pass.SetPipeline(mPipeline);
        pass.SetVertexBuffer(0, mVertexBuffer);
        wgpu::BindGroup bindGroup = utils::MakeBindGroup(device, mUniformBindGroupLayout,
                                                         {{0, uniformBuffer, 0, GetBufferSize()}});
        pass.SetBindGroup(0, bindGroup);
        pass.Draw(3);
        pass.End();
        wgpu::CommandBuffer commandBuffer = commands.Finish();
        queue.Submit(1, &commandBuffer);
        if (GetParam().uploadMethod == UploadMethod::StagingBuffer) {
            CallbackData* callbackData = new CallbackData({this, stagingBuffer});
            stagingBuffer.MapAsync(
                wgpu::MapMode::Write, 0, GetBufferSize(),
                [](WGPUBufferMapAsyncStatus status, void* userdata) {
                    CallbackData* data = static_cast<CallbackData*>(userdata);
                    if (status == WGPUBufferMapAsyncStatus::WGPUBufferMapAsyncStatus_Success) {
                        data->self->ReturnStagingBuffer(data->buffer);
                    }
                    delete data;
                },
                callbackData);
        }
        CallbackData* callbackData = new CallbackData({this, uniformBuffer});
        queue.OnSubmittedWorkDone(
            [](WGPUQueueWorkDoneStatus status, void* userdata) {
                CallbackData* data = static_cast<CallbackData*>(userdata);
                if (status == WGPUQueueWorkDoneStatus::WGPUQueueWorkDoneStatus_Success) {
                    data->self->ReturnUniformBuffer(data->buffer);
                }
                delete data;
            },
            callbackData);
    }
}

TEST_P(UniformBufferUpdatePerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(UniformBufferUpdatePerf,
                        {D3D11Backend(), D3D12Backend(), MetalBackend(), OpenGLBackend(),
                         VulkanBackend()},
                        {UploadMethod::WriteBuffer, UploadMethod::StagingBuffer},
                        {UploadSize::Partial, UploadSize::Full});

}  // anonymous namespace
}  // namespace dawn
