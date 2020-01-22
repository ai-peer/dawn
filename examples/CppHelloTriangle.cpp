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

#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include "GLFW/glfw3.h"
#include "Windows.h"
#include "utils/ComboRenderPipelineDescriptor.h"
#include "utils/SystemUtils.h"
#include "utils/Timer.h"
#include "utils/WGPUHelpers.h"

uint32_t frameNumber = 0;

wgpu::Device device;
int drawStart = 0;
wgpu::Buffer indexBuffer;
wgpu::Buffer indexBuffer2;
wgpu::Buffer vertexBuffer;
wgpu::Buffer vertexOffsetBuffer;
wgpu::Buffer offsetBuffer;

std::vector<double> darray;
std::vector<wgpu::Texture> textures;
std::vector<wgpu::Sampler> samplers;
std::vector<wgpu::BindGroup> bindGroups;

std::vector<float> vertexOffsets;
wgpu::Queue queue;
wgpu::SwapChain swapchain;
wgpu::TextureView depthStencilView;
wgpu::RenderPipeline pipeline;
utils::Timer* mTimer;

// Amount of resources to create in MB
#define RESOURCE_POOL_SIZE 2500
#define BUDGET_PER_FRAME 500
#define RESOURCE_SET_SIZE 25

void initBuffers() {
    static const uint32_t indexData[6] = {0, 1, 2, 3, 4, 5};
    indexBuffer =
        utils::CreateBufferFromData(device, indexData, sizeof(indexData), wgpu::BufferUsage::Index);

    static const float vertexData[18] = {
        -1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  0.0f, 1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f,
    };

    vertexBuffer = utils::CreateBufferFromData(device, vertexData, sizeof(vertexData),
                                               wgpu::BufferUsage::Vertex);

    for (int i = 0; i <= RESOURCE_POOL_SIZE * 256; i++) {
        vertexOffsets.push_back(320 - (std::rand() % 640));
        vertexOffsets.push_back(240 - (std::rand() % 480));
        vertexOffsets.push_back(0);
        vertexOffsets.push_back(0);
        vertexOffsets.push_back(0);
        vertexOffsets.push_back(0);
        vertexOffsets.push_back(0);
        vertexOffsets.push_back(0);
    }
    size_t size = vertexOffsets.size();

    wgpu::BufferDescriptor descriptor;
    descriptor.size = vertexOffsets.size();
    descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;

    vertexOffsetBuffer = device.CreateBuffer(&descriptor);
    vertexOffsetBuffer.SetSubData(0, size, vertexOffsets.data());
}

void initTextures() {
    for (int i = 0; i < RESOURCE_POOL_SIZE; i++) {
        // Create textures 1MB in size each
        wgpu::TextureDescriptor descriptor;
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size.width = 512;
        descriptor.size.height = 512;
        descriptor.size.depth = 1;
        descriptor.arrayLayerCount = 1;
        descriptor.sampleCount = 1;
        descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
        descriptor.mipLevelCount = 1;
        descriptor.usage = wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::Sampled;
        textures.push_back(device.CreateTexture(&descriptor));

        // Create a sampler for each texture
        wgpu::SamplerDescriptor samplerDesc = utils::GetDefaultSamplerDescriptor();
        samplers.push_back(device.CreateSampler(&samplerDesc));

        // Initialize each texture with unique data
        std::vector<uint32_t> data(4 * 512 * 512, 0);
        uint32_t randomColor = static_cast<uint8_t>(255);
        for (int k = 0; k < 3; k++) {
            randomColor = randomColor << 8;
            randomColor += static_cast<uint8_t>(std::rand() % 255);
        }
        for (size_t j = 0; j < data.size(); j++) {
            data[j] = randomColor;
        }

        wgpu::Buffer stagingBuffer = utils::CreateBufferFromData(
            device, data.data(), static_cast<uint32_t>(data.size()), wgpu::BufferUsage::CopySrc);
        wgpu::BufferCopyView bufferCopyView = utils::CreateBufferCopyView(stagingBuffer, 0, 0, 0);
        wgpu::TextureCopyView textureCopyView =
            utils::CreateTextureCopyView(textures[i], 0, 0, {0, 0, 0});
        wgpu::Extent3D copySize = {512, 512, 1};

        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToTexture(&bufferCopyView, &textureCopyView, &copySize);

        wgpu::CommandBuffer copy = encoder.Finish();
        queue.Submit(1, &copy);
        DoFlush();
    }
}

void init() {
    device = CreateCppDawnDevice();
    mTimer = utils::CreateTimer();
    queue = device.CreateQueue();
    swapchain = GetSwapChain(device);
    swapchain.Configure(GetPreferredSwapChainTextureFormat(), wgpu::TextureUsage::OutputAttachment,
                        640, 480);

    initBuffers();
    initTextures();
    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, R"(
        #version 450
        layout(location = 0) in vec3 pos;
        void main() {
            gl_Position = vec4(pos.xy, 0.0f, 1.0f);
        })");

    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, R"(
        #version 450
        layout(set = 0, binding = 0) uniform sampler mySampler;
        layout(set = 0, binding = 1) uniform texture2D myTexture;

        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = texture(sampler2D(myTexture, mySampler), gl_FragCoord.xy / vec2(640.0, 480.0));
        })");

    auto bgl = utils::MakeBindGroupLayout(
        device, {{0, wgpu::ShaderStage::Fragment, wgpu::BindingType::Sampler},
                 {1, wgpu::ShaderStage::Fragment, wgpu::BindingType::SampledTexture}});

    for (int i = 0; i < RESOURCE_POOL_SIZE; i++) {
        wgpu::TextureView view = textures[i].CreateView();
        bindGroups.push_back(utils::MakeBindGroup(device, bgl, {{0, samplers[i]}, {1, view}}));
    }
    wgpu::PipelineLayout pl = utils::MakeBasicPipelineLayout(device, &bgl);

    depthStencilView = CreateDefaultDepthStencilView(device);

    utils::ComboRenderPipelineDescriptor descriptor(device);
    descriptor.layout = utils::MakeBasicPipelineLayout(device, &bgl);
    descriptor.vertexStage.module = vsModule;
    descriptor.cFragmentStage.module = fsModule;
    descriptor.cVertexState.vertexBufferCount = 1;
    descriptor.cVertexState.cVertexBuffers[0].arrayStride = 3 * sizeof(float);
    descriptor.cVertexState.cVertexBuffers[0].attributeCount = 1;
    descriptor.cVertexState.cAttributes[0].format = wgpu::VertexFormat::Float3;
    descriptor.depthStencilState = &descriptor.cDepthStencilState;
    descriptor.cDepthStencilState.format = wgpu::TextureFormat::Depth24PlusStencil8;
    descriptor.cColorStates[0].format = GetPreferredSwapChainTextureFormat();

    pipeline = device.CreateRenderPipeline(&descriptor);
}

struct {
    uint32_t a;
    float b;
} s;

void frame() {
    s.a = (s.a + 1) % 256;
    s.b += 0.02f;
    if (s.b >= 1.0f) {
        s.b = 0.0f;
    }
    wgpu::TextureView backbufferView = swapchain.GetCurrentTextureView();
    utils::ComboRenderPassDescriptor renderPass({backbufferView}, depthStencilView);

    //  static const uint64_t vertexBufferOffsets[1] = {0};
    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    {
        wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPass);
        pass.SetPipeline(pipeline);
        int index = drawStart;
        for (int i = 0; i < BUDGET_PER_FRAME; i++) {
            pass.SetBindGroup(0, bindGroups[index], 0, nullptr);
            pass.SetIndexBuffer(indexBuffer, 0);
            pass.SetVertexBuffer(0, vertexBuffer);
            pass.DrawIndexed(6, 1, 0, 0, 0);
            index++;
            if (index >= RESOURCE_POOL_SIZE) {
                index = 0;
            }
        }
        pass.EndPass();
    }

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);
    swapchain.Present();
    DoFlush();
    drawStart += RESOURCE_SET_SIZE;
    if (drawStart >= RESOURCE_POOL_SIZE) {
        drawStart = 0;
    }
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }
    init();

    while (!ShouldQuit()) {
        frame();
        mTimer->Stop();
        double frameTime = mTimer->GetElapsedTime();
        double fps = 1 / frameTime;
        darray.emplace(darray.begin(), fps);
        double average = 0;
        if (darray.size() >= 20) {
            darray.pop_back();
            for (int i = 0; i < 20; i++) {
                average += darray[i];
            }
        }
        average /= darray.size();
        std::string s = std::to_string(average);
        glfwSetWindowTitle(GetGLFWWindow(), s.c_str());
        mTimer->Start();
        // utils::USleep(1);
    }

    // TODO release stuff
}