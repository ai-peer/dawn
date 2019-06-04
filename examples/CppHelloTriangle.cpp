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

#include "SampleUtils.h"

#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/DawnHelpers.h"
#include "utils/SystemUtils.h"

#include <vector>

dawn::Device device;

dawn::Buffer indexBuffer;
dawn::Buffer vertexBuffer;

dawn::Texture texture;
dawn::Sampler sampler;

dawn::Queue queue;
dawn::SwapChain swapchain;
dawn::TextureView depthStencilView;
dawn::RenderPipeline pipeline;
dawn::BindGroup bindGroup;

void initBuffers() {
    static const uint32_t indexData[6] = {0, 1, 2, 2, 0, 3};
    indexBuffer = utils::CreateBufferFromData(device, indexData, sizeof(indexData),
                                              dawn::BufferUsageBit::Index);

    static const float vertexData[16] = {-1.0f, 1.0f,  0.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f,
                                         1.0f,  -1.0f, 0.0f, 1.0f, 1.0f,  1.0f,  0.0f, 1.0f};
    vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                               dawn::BufferUsageBit::Vertex);
}

constexpr uint32_t kBlockWidthCount = 2;
constexpr uint32_t kBlockHeightCount = 2;

constexpr uint32_t kBCBlockWidth = 4;
constexpr uint32_t kBCBlockHeight = 4;

void fillUploadData(std::array<uint8_t, kTextureRowPitchAlignment * kBlockHeightCount>* uploadData,
                    dawn::TextureFormat format) {
    // The pixel data represents a 4x4 pixel image with the left side colored red and the right side
    // green. It was BC7 encoded using Microsoft's BC6HBC7Encoder.
    constexpr std::array<uint8_t, kBCBlockWidth* kBCBlockHeight> kBC7Data4x4 = {
        0x50, 0x1f, 0xfc, 0xf, 0x0, 0xf0, 0xe3, 0xe1, 0xe1, 0xe1, 0xc1, 0xf, 0xfc, 0xc0, 0xf,  0xfc};

    constexpr std::array<uint8_t, kBCBlockWidth * kBCBlockHeight> kBC1Data = {
        0, 248, 224, 7, 80, 80, 80, 80
    };

    constexpr std::array<uint8_t, kBCBlockWidth * kBCBlockHeight> kBC5SNORMData = {
        127, 129, 64, 2, 36, 64, 2, 36, 127, 129, 9, 144, 0, 9, 144, 0
    };

    constexpr std::array<uint8_t, kBCBlockWidth * kBCBlockHeight> kBC5UNORMData = {
        255, 0, 64, 2, 36, 64, 2, 36, 255, 0, 9, 144, 0, 9, 144, 0
    };

    constexpr std::array<uint8_t, 16> kBC6HSFloatData = {
        227, 30, 0, 0, 0, 224, 30, 0, 0, 255, 0, 255, 0, 255, 0, 255
    };
    constexpr std::array<uint8_t, 16> kBC6HUFloatData = {
        227, 61, 0, 0, 0, 224, 61, 0, 0, 255, 0, 255, 0, 255, 0, 255
    };

    const uint8_t* compressedDataPtr = nullptr;
    uint32_t compressedDataSize = 0;

    switch (format) {
        case dawn::TextureFormat::BC7RGBAUnorm:
            compressedDataPtr = kBC7Data4x4.data();
            compressedDataSize = kBC7Data4x4.size();
            break;
        case dawn::TextureFormat::BC1RGBAUnorm:
            compressedDataPtr = kBC1Data.data();
            compressedDataSize = 8;
            break;
        case dawn::TextureFormat::BC5RGSnorm:
            compressedDataPtr = kBC5SNORMData.data();
            compressedDataSize = 16;
            break;
        case dawn::TextureFormat::BC5RGUnorm:
            compressedDataPtr = kBC5UNORMData.data();
            compressedDataSize = 16;
            break;
        case dawn::TextureFormat::BC6HRGBSfloat:
            compressedDataPtr = kBC6HSFloatData.data();
            compressedDataSize = 16;
            break;
        case dawn::TextureFormat::BC6HRGBUfloat:
            compressedDataPtr = kBC6HUFloatData.data();
            compressedDataSize = 16;
            break;
        default:
            compressedDataPtr = kBC7Data4x4.data();
            compressedDataSize = kBC7Data4x4.size();
            break;
    }

    for (uint32_t i = 0; i < kBlockHeightCount; ++i) {
        for (uint32_t j = 0; j < kBlockWidthCount; ++j) {
            for (uint32_t k = 0; k < compressedDataSize; ++k) {
                (*uploadData)[kTextureRowPitchAlignment * i + compressedDataSize * j + k] = compressedDataPtr[k];
            }
        }
    }
}

void initTextures(dawn::TextureFormat format) {
    dawn::TextureDescriptor descriptor;
    descriptor.dimension = dawn::TextureDimension::e2D;
    descriptor.size.width = kBCBlockWidth * kBlockWidthCount;
    descriptor.size.height = kBCBlockHeight * kBlockHeightCount;
    descriptor.size.depth = 1;
    descriptor.arrayLayerCount = 1;
    descriptor.sampleCount = 1;
    descriptor.format = format;
    descriptor.mipLevelCount = 1;
    descriptor.usage = dawn::TextureUsageBit::TransferDst | dawn::TextureUsageBit::Sampled;
    texture = device.CreateTexture(&descriptor);

    dawn::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
    samplerDesc.minFilter = dawn::FilterMode::Nearest;
    samplerDesc.magFilter = dawn::FilterMode::Nearest;
    sampler = device.CreateSampler(&samplerDesc);


    std::array<uint8_t, kTextureRowPitchAlignment * kBlockHeightCount> uploadData;
    for (uint32_t i = 0; i < kTextureRowPitchAlignment; ++i) {
        uploadData[i] = 1;
    }
    for (uint32_t i = 0; i < kTextureRowPitchAlignment; ++i) {
        uploadData[kTextureRowPitchAlignment + i] = 2;
    }
    fillUploadData(&uploadData, format);

    dawn::Buffer stagingBuffer = utils::CreateBufferFromData(
        device, uploadData.data(), static_cast<uint32_t>(uploadData.size()),
                                    dawn::BufferUsageBit::TransferSrc);
    dawn::BufferCopyView bufferCopyView =
        utils::CreateBufferCopyView(stagingBuffer, 0, kTextureRowPitchAlignment, 0);
    dawn::TextureCopyView textureCopyView = utils::CreateTextureCopyView(texture, 0, 0, {0, 0, 0});
    dawn::Extent3D copySize = {kBlockWidthCount * kBCBlockWidth, kBlockHeightCount * kBCBlockHeight,
                               1};

    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);

    dawn::CommandBuffer copy = encoder.Finish();
    queue.Submit(1, &copy);
}

void init() {
    device = CreateCppDawnDevice();

    queue = device.CreateQueue();
    swapchain = GetSwapChain(device);
    swapchain.Configure(GetPreferredSwapChainTextureFormat(),
                        dawn::TextureUsageBit::OutputAttachment, 640, 480);

    initBuffers();
    //initTextures(dawn::TextureFormat::BC7RGBAUnorm);
    //initTextures(dawn::TextureFormat::BC1RGBAUnorm);
    //initTextures(dawn::TextureFormat::BC5RGSnorm);
    //initTextures(dawn::TextureFormat::BC5RGUnorm);

    //initTextures(dawn::TextureFormat::BC6HRGBSfloat);
    initTextures(dawn::TextureFormat::BC6HRGBUfloat);

    dawn::ShaderModule vsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Vertex, R"(
        #version 450
        layout(location = 0) in vec4 pos;
        void main() {
            gl_Position = pos;
        })"
    );

    dawn::ShaderModule fsModule = utils::CreateShaderModule(device, dawn::ShaderStage::Fragment, R"(
        #version 450
        layout(set = 0, binding = 0) uniform sampler mySampler;
        layout(set = 0, binding = 1) uniform texture2D myTexture;

        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = texture(sampler2D(myTexture, mySampler), gl_FragCoord.xy / vec2(640.0, 480.0));
        })");

    auto bgl = utils::MakeBindGroupLayout(
        device, {
                    {0, dawn::ShaderStageBit::Fragment, dawn::BindingType::Sampler},
                    {1, dawn::ShaderStageBit::Fragment, dawn::BindingType::SampledTexture},
                });

    dawn::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    depthStencilView = CreateDefaultDepthStencilView(device);

    utils::ComboRenderPipelineDescriptor descriptor(device);
    descriptor.layout = utils::MakeBasicPipelineLayout(device, &bgl);
    descriptor.cVertexStage.module = vsModule;
    descriptor.cFragmentStage.module = fsModule;
    descriptor.cVertexInput.numAttributes = 1;
    descriptor.cVertexInput.cAttributes[0].format = dawn::VertexFormat::Float4;
    descriptor.cVertexInput.numBuffers = 1;
    descriptor.cVertexInput.cBuffers[0].stride = 4 * sizeof(float);
    descriptor.depthStencilState = &descriptor.cDepthStencilState;
    descriptor.cDepthStencilState.format = dawn::TextureFormat::D32FloatS8Uint;
    descriptor.cColorStates[0]->format = GetPreferredSwapChainTextureFormat();

    pipeline = device.CreateRenderPipeline(&descriptor);

    dawn::TextureView view = texture.CreateDefaultView();

    bindGroup = utils::MakeBindGroup(device, bgl, {
        {0, sampler},
        {1, view}
    });
}

struct {uint32_t a; float b;} s;
void frame() {
    s.a = (s.a + 1) % 256;
    s.b += 0.02f;
    if (s.b >= 1.0f) {s.b = 0.0f;}

    dawn::Texture backbuffer = swapchain.GetNextTexture();
    utils::ComboRenderPassDescriptor renderPass({backbuffer.CreateDefaultView()},
                                                depthStencilView);

    static const uint64_t vertexBufferOffsets[1] = {0};
    dawn::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        dawn::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup, 0, nullptr);
        pass.SetVertexBuffers(0, 1, &vertexBuffer, vertexBufferOffsets);
        pass.SetIndexBuffer(indexBuffer, 0);
        pass.DrawIndexed(6, 1, 0, 0, 0);
        pass.EndPass();
    }

    dawn::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
    swapchain.Present(backbuffer);
    DoFlush();
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();

    while (!ShouldQuit()) {
        frame();
        utils::USleep(16000);
    }

    // TODO release stuff
}
