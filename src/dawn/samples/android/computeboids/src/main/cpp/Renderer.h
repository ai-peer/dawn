// Copyright 2023 The Dawn Authors
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
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <array>
#include <cstring>
#include <random>
#include <vector>

#include "dawn/samples/utils/SampleUtils.h"

#include "dawn/utils/ComboRenderPipelineDescriptor.h"
#include "dawn/utils/SystemUtils.h"
#include "dawn/utils/WGPUHelpers.h"

#ifndef APP_SRC_MAIN_CPP_RENDERER_H_
#define APP_SRC_MAIN_CPP_RENDERER_H_

class Renderer {
  public:
    wgpu::Device device;
    wgpu::SwapChain swapchain;
    wgpu::RenderPipeline pipeline;
    wgpu::Queue queue;
    wgpu::SurfaceDescriptorFromAndroidNativeWindow androidDesc;
    wgpu::Instance instance;
    wgpu::InstanceDescriptor desc;
    bool deviceInitialised;

    wgpu::TextureView depthStencilView;

    wgpu::Buffer modelBuffer;
    std::array<wgpu::Buffer, 2> particleBuffers;

    wgpu::RenderPipeline renderPipeline;

    wgpu::Buffer updateParams;
    wgpu::ComputePipeline updatePipeline;
    std::array<wgpu::BindGroup, 2> updateBGs;

    size_t pingpong = 0;

    static const uint32_t kNumParticles = 1024;

    struct Particle {
        std::array<float, 2> pos;
        std::array<float, 2> vel;
    };

    struct SimParams {
        float deltaT;
        float rule1Distance;
        float rule2Distance;
        float rule3Distance;
        float rule1Scale;
        float rule2Scale;
        float rule3Scale;
        int particleCount;
    };

    explicit Renderer(android_app* app) {
        deviceInitialised = false;
        androidDesc.window = app->window;
        androidDesc.sType = wgpu::SType::SurfaceDescriptorFromAndroidNativeWindow;
    }
    void Init();
    void InitBuffers();
    void InitRender();
    void InitSim();
    wgpu::CommandBuffer createCommandBuffer(const wgpu::TextureView backbufferView, size_t i);
    void Frame();
    void GameLoop();
};

#endif  // APP_SRC_MAIN_CPP_RENDERER_H_
