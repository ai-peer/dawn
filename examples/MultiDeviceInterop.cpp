// Copyright 2020 The Dawn Authors
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

#include "common/Platform.h"
#include "utils/SystemUtils.h"
#include "utils/WGPUHelpers.h"

#include <unistd.h>
#include <cmath>

#if defined(DAWN_PLATFORM_MACOS)
#    include "dawn_native/MetalBackend.h"

#    include <CoreFoundation/CoreFoundation.h>
#    include <CoreVideo/CVPixelBuffer.h>
#    include <IOSurface/IOSurface.h>
#endif

namespace {
#if defined(DAWN_PLATFORM_MACOS)
    void AddIntegerValue(CFMutableDictionaryRef dictionary, const CFStringRef key, int32_t value) {
        CFNumberRef number = CFNumberCreate(nullptr, kCFNumberSInt32Type, &value);
        CFDictionaryAddValue(dictionary, key, number);
        CFRelease(number);
    }
#endif
}  // namespace

wgpu::Device device;
wgpu::Device secondDevice;
wgpu::Queue queue;
wgpu::SwapChain swapchain;
wgpu::RenderPipeline pipeline;
wgpu::Buffer colorBuffer;
wgpu::BindGroup uniformBindGroup;

wgpu::TextureFormat swapChainFormat;

void initParent() {
    device = CreateCppDawnDevice();
    queue = device.GetDefaultQueue();

    {
        wgpu::SwapChainDescriptor descriptor = {};
        descriptor.implementation = GetSwapChainImplementation();
        swapchain = device.CreateSwapChain(nullptr, &descriptor);
    }
    swapChainFormat = static_cast<wgpu::TextureFormat>(GetPreferredSwapChainTextureFormat());
    swapchain.Configure(swapChainFormat, wgpu::TextureUsage::OutputAttachment, 640, 480);
}

void initChild() {
    device = CreateCppDawnDevice();
    queue = device.GetDefaultQueue();

    // {
    //     wgpu::SwapChainDescriptor descriptor = {};
    //     descriptor.implementation = GetSwapChainImplementation();
    //     swapchain = device.CreateSwapChain(nullptr, &descriptor);
    // }
    swapChainFormat = static_cast<wgpu::TextureFormat>(GetPreferredSwapChainTextureFormat());
    // swapchain.Configure(swapChainFormat, wgpu::TextureUsage::OutputAttachment, 640, 480);

    const char* vs = R"(
        #version 450
        const vec2 pos[3] = vec2[3](vec2(0.0f, 0.5f), vec2(-0.5f, -0.5f), vec2(0.5f, -0.5f));
        void main() {
           gl_Position = vec4(pos[gl_VertexIndex], 0.0, 1.0);
        })";
    wgpu::ShaderModule vsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Vertex, vs);

    const char* fs = R"(
        #version 450
        layout(set = 0, binding = 0) uniform Uniforms {
            vec3 color;
        };
        layout(location = 0) out vec4 fragColor;
        void main() {
           fragColor = vec4(color, 1.0);
        })";
    wgpu::ShaderModule fsModule =
        utils::CreateShaderModule(device, utils::SingleShaderStage::Fragment, fs);

    {
        wgpu::RenderPipelineDescriptor descriptor = {};

        descriptor.vertexStage.module = vsModule;
        descriptor.vertexStage.entryPoint = "main";

        wgpu::ProgrammableStageDescriptor fragmentStage = {};
        fragmentStage.module = fsModule;
        fragmentStage.entryPoint = "main";
        descriptor.fragmentStage = &fragmentStage;

        wgpu::ColorStateDescriptor colorStateDescriptor = {};
        colorStateDescriptor.format = swapChainFormat;

        descriptor.colorStateCount = 1;
        descriptor.colorStates = &colorStateDescriptor;

        descriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleList;

        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    colorBuffer = utils::CreateBufferFromData(
        device, wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, {1.f, 0.f, 0.f});

    wgpu::BindGroupEntry colorBufferEntry = {};
    colorBufferEntry.binding = 0;
    colorBufferEntry.buffer = colorBuffer;
    colorBufferEntry.size = 3 * sizeof(float);

    wgpu::BindGroupDescriptor bgDesc = {};
    bgDesc.layout = pipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 1;
    bgDesc.entries = &colorBufferEntry;

    uniformBindGroup = device.CreateBindGroup(&bgDesc);
}

static float frameNumber = 0.f;
void frameParent() {
    wgpu::TextureView backbufferView = swapchain.GetCurrentTextureView();
    swapchain.Present();

    DoFlush();
    frameNumber++;
}

void frameChild() {
    wgpu::Texture externalTexture;
#if defined(DAWN_PLATFORM_MACOS)
    {
        CFMutableDictionaryRef dict =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        AddIntegerValue(dict, kIOSurfaceWidth, 640);
        AddIntegerValue(dict, kIOSurfaceHeight, 480);
        AddIntegerValue(dict, kIOSurfacePixelFormat, kCVPixelFormatType_32BGRA);
        AddIntegerValue(dict, kIOSurfaceBytesPerElement, 4);

        IOSurfaceRef ioSurface = IOSurfaceCreate(dict);
        ASSERT(ioSurface != nullptr);
        CFRelease(dict);

        wgpu::TextureDescriptor backbufferDesc = {};
        backbufferDesc.dimension = wgpu::TextureDimension::e2D;
        backbufferDesc.format = wgpu::TextureFormat::BGRA8Unorm;
        backbufferDesc.size = {640, 480, 1};
        backbufferDesc.sampleCount = 1;
        backbufferDesc.mipLevelCount = 1;
        backbufferDesc.usage = wgpu::TextureUsage::OutputAttachment;

        dawn_native::metal::ExternalImageDescriptorIOSurface externDesc;
        externDesc.cTextureDescriptor =
            reinterpret_cast<const WGPUTextureDescriptor*>(&backbufferDesc);
        externDesc.ioSurface = ioSurface;
        externDesc.plane = 0;
        externDesc.isInitialized = false;

        externalTexture =
            wgpu::Texture::Acquire(dawn_native::metal::WrapIOSurface(device.Get(), &externDesc));

        CFRelease(ioSurface);
    }
#endif

    wgpu::RenderPassDescriptor renderpassInfo = {};
    wgpu::RenderPassColorAttachmentDescriptor colorAttachment = {};
    {
        colorAttachment.attachment = externalTexture.CreateView();
        colorAttachment.clearColor = {0.0f, 0.0f, 0.0f, 0.0f};
        renderpassInfo.colorAttachmentCount = 1;
        renderpassInfo.colorAttachments = &colorAttachment;
    }

    std::array<float, 3> color = {
        std::abs(std::sin(frameNumber / 60.f + 0.f)),
        std::abs(std::sin(frameNumber / 60.f + 1.57f)),
        std::abs(std::sin(frameNumber / 60.f + 3.14f)),
    };
    queue.WriteBuffer(colorBuffer, 0, color.data(), sizeof(float) * color.size());

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpassInfo);
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, uniformBindGroup);
    pass.Draw(3);
    pass.EndPass();

    wgpu::CommandBuffer commands = encoder.Finish();
    queue.Submit(1, &commands);

    DoFlush();
    frameNumber++;
}

int main(int argc, const char* argv[]) {
    if (!InitSample(argc, argv)) {
        return 1;
    }

    bool isChild = fork() == 0;
    if (isChild) {
        initChild();
        while (!ShouldQuit()) {
            frameChild();
            utils::USleep(16000);
        }
    } else {
        initParent();
        while (!ShouldQuit()) {
            frameParent();
            utils::USleep(16000);
        }
    }

    // TODO release stuff
}
