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

#include "dawn/common/Math.h"
#include "dawn/common/MutexProtected.h"
#include "dawn/tests/perf_tests/DawnPerfTest.h"
#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/WGPUHelpers.h"

// This is for developers only to check the texels copied into the buffer.
// #define PIXEL_CHECK 1

namespace dawn {
namespace {

constexpr unsigned int kNumIterations = 100;

// Draw a triangle.
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
        @fragment fn main() -> @location(0) vec4f {
            return vec4f(0.2, 0.4, 0.8, 1.0);
        })";

enum class Size { Small, Medium, Large };

struct TextureReadParams : AdapterTestParam {
    TextureReadParams(const AdapterTestParam& param, Size size)
        : AdapterTestParam(param), size(size) {}

    Size size;
};

std::ostream& operator<<(std::ostream& ostream, const TextureReadParams& param) {
    ostream << static_cast<const AdapterTestParam&>(param);

    switch (param.size) {
        case Size::Small:
            ostream << "_SmallSize";
            break;
        case Size::Medium:
            ostream << "_MediumSize";
            break;
        case Size::Large:
            ostream << "_LargeSize";
            break;
    }

    return ostream;
}

// Test copying texture to buffer for readback |kNumIterations| times.
class TextureReadPerf : public DawnPerfTestWithParams<TextureReadParams> {
  public:
    TextureReadPerf() : DawnPerfTestWithParams(kNumIterations, 1) {}
    ~TextureReadPerf() override = default;

    void SetUp() override;

  private:
    // Data needed for resource returning.
    struct CallbackData {
        TextureReadPerf* self;
        wgpu::Texture texture;
        wgpu::Buffer buffer;
    };

    void Step() override;
    wgpu::Buffer FindOrCreateBuffer();
    void ReturnBuffer(wgpu::Buffer buffer);
    wgpu::Texture FindOrCreateTexture();
    void ReturnTexture(wgpu::Texture texture);

    size_t mBufferSize = 0;
#ifdef PIXEL_CHECK
    uint64_t mOffsetOfTriangeCenter = 0;
#endif

    wgpu::Texture mColorAttachmentTexture;
    wgpu::Buffer mVertexBuffer;
    wgpu::RenderPipeline mPipeline;
    wgpu::TextureDescriptor mTextureDesc;

    MutexProtected<std::queue<wgpu::Buffer>> mBuffers;
    MutexProtected<std::queue<wgpu::Texture>> mTextures;

    wgpu::ImageCopyTexture mImageCopyTexture;
    wgpu::ImageCopyBuffer mImageCopyBuffer;
    wgpu::Extent3D mCopySize;
};

// Try to grab a free buffer. If unavailable, create a new one on-the-fly.
wgpu::Buffer TextureReadPerf::FindOrCreateBuffer() {
    if (!mBuffers->empty()) {
        wgpu::Buffer buffer = mBuffers->front();
        mBuffers->pop();
        return buffer;
    }
    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
#ifdef PIXEL_CHECK
    desc.usage = wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst;
#endif
    desc.size = mBufferSize;
    return device.CreateBuffer(&desc);
}

// Return a buffer, so that it's free to be re-used.
void TextureReadPerf::ReturnBuffer(wgpu::Buffer buffer) {
    mBuffers->push(buffer);
}

// Try to grab a free texture. If unavailable, create a new one on-the-fly.
wgpu::Texture TextureReadPerf::FindOrCreateTexture() {
    if (!mTextures->empty()) {
        wgpu::Texture texture = mTextures->front();
        mTextures->pop();
        return texture;
    }

    return device.CreateTexture(&mTextureDesc);
}

// Return a texture, so that it's free to be re-used.
void TextureReadPerf::ReturnTexture(wgpu::Texture texture) {
    mTextures->push(texture);
}

void TextureReadPerf::SetUp() {
    DawnPerfTestWithParams<TextureReadParams>::SetUp();

    // Create the vertex buffer
    mVertexBuffer = utils::CreateBufferFromData(device, kVertexData, sizeof(kVertexData),
                                                wgpu::BufferUsage::Vertex);

    // Setup the base render pipeline descriptor.
    utils::ComboRenderPipelineDescriptor renderPipelineDesc;
    renderPipelineDesc.vertex.bufferCount = 1;
    renderPipelineDesc.cBuffers[0].arrayStride = 4 * sizeof(float);
    renderPipelineDesc.cBuffers[0].attributeCount = 1;
    renderPipelineDesc.cAttributes[0].format = wgpu::VertexFormat::Float32x4;
    renderPipelineDesc.cTargets[0].format = wgpu::TextureFormat::RGBA8Unorm;

    // Create the pipeline layout for the pipeline.
    wgpu::PipelineLayoutDescriptor pipelineLayoutDesc = {};
    wgpu::PipelineLayout pipelineLayout = device.CreatePipelineLayout(&pipelineLayoutDesc);

    // Create the shaders for the pipeline.
    wgpu::ShaderModule vsModule = utils::CreateShaderModule(device, kVertexShader);
    wgpu::ShaderModule fsModule = utils::CreateShaderModule(device, kFragmentShader);

    // Create the pipeline.
    renderPipelineDesc.layout = pipelineLayout;
    renderPipelineDesc.vertex.module = vsModule;
    renderPipelineDesc.cFragment.module = fsModule;
    mPipeline = device.CreateRenderPipeline(&renderPipelineDesc);

    mTextureDesc.dimension = wgpu::TextureDimension::e2D;
    mTextureDesc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
    mTextureDesc.format = wgpu::TextureFormat::RGBA8Unorm;

    mTextureDesc.size.depthOrArrayLayers = 1;
    switch (GetParam().size) {
        case Size::Small:
            mTextureDesc.size.width = 64;
            mTextureDesc.size.height = 64;
            break;
        case Size::Medium:
            mTextureDesc.size.width = 1280;
            mTextureDesc.size.height = 720;
            break;
        case Size::Large:
            mTextureDesc.size.width = 1920;
            mTextureDesc.size.height = 1080;
            break;
    }
    uint32_t texelBlockSize = utils::GetTexelBlockSizeInBytes(mTextureDesc.format);
    uint32_t bytesPerRow =
        Align(mTextureDesc.size.width * texelBlockSize, kTextureBytesPerRowAlignment);
    uint32_t rowsPerImage = mTextureDesc.size.height;
    mBufferSize = utils::RequiredBytesInCopy(bytesPerRow, rowsPerImage, mTextureDesc.size.width,
                                             mTextureDesc.size.height, 1, texelBlockSize);
#ifdef PIXEL_CHECK
    mOffsetOfTriangeCenter =
        bytesPerRow * (desc.size.height / 2) + texelBlockSize * (desc.size.width / 2);
#endif

    mImageCopyTexture = utils::CreateImageCopyTexture(nullptr);
    mImageCopyBuffer = utils::CreateImageCopyBuffer(nullptr, 0, bytesPerRow, rowsPerImage);
    mCopySize = {mTextureDesc.size.width, mTextureDesc.size.height, 1};
}

void TextureReadPerf::Step() {
    for (unsigned int i = 0; i < kNumIterations; ++i) {
        wgpu::CommandEncoder commands = device.CreateCommandEncoder();
        wgpu::Texture texture = FindOrCreateTexture();
        utils::ComboRenderPassDescriptor renderPass({texture.CreateView()});
        wgpu::RenderPassEncoder pass = commands.BeginRenderPass(&renderPass);
        pass.SetPipeline(mPipeline);
        pass.SetVertexBuffer(0, mVertexBuffer);
        pass.Draw(3);
        pass.End();
        wgpu::Buffer buffer = FindOrCreateBuffer();
        mImageCopyTexture.texture = texture;
        mImageCopyBuffer.buffer = buffer;
        commands.CopyTextureToBuffer(&mImageCopyTexture, &mImageCopyBuffer, &mCopySize);
        wgpu::CommandBuffer commandBuffer = commands.Finish();
        queue.Submit(1, &commandBuffer);
#ifdef PIXEL_CHECK
        uint32_t expected[] = {0xFFCC6633};
        EXPECT_BUFFER_U32_RANGE_EQ(expected, buffer, mOffsetOfTriangeCenter, 1);
#endif
        CallbackData* callbackData = new CallbackData({this, texture, buffer});
        queue.OnSubmittedWorkDone(
            [](WGPUQueueWorkDoneStatus status, void* userdata) {
                CallbackData* data = static_cast<CallbackData*>(userdata);
                data->self->ReturnBuffer(data->buffer);
                data->self->ReturnTexture(data->texture);
                delete data;
            },
            callbackData);
    }
}

TEST_P(TextureReadPerf, Run) {
    RunTest();
}

DAWN_INSTANTIATE_TEST_P(TextureReadPerf,
                        {D3D11Backend(), D3D12Backend(), MetalBackend(), OpenGLBackend(),
                         OpenGLESBackend(), VulkanBackend()},
                        {Size::Small, Size::Medium, Size::Large});

}  // anonymous namespace
}  // namespace dawn
