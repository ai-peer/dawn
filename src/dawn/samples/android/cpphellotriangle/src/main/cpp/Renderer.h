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
#include "dawn/samples/SampleUtils.h"

#ifndef APP_SRC_MAIN_CPP_RENDERER_H_
#define APP_SRC_MAIN_CPP_RENDERER_H_

class Renderer {
  public:
    wgpu::Device device;
    wgpu::SwapChain swapChain;
    wgpu::RenderPipeline pipeline;
    wgpu::Queue queue;
    wgpu::SurfaceDescriptorFromAndroidNativeWindow androidDesc;
    wgpu::Instance instance;
    wgpu::InstanceDescriptor desc;
    bool deviceInitialised;

    explicit Renderer(android_app* app) {
        deviceInitialised = false;
        androidDesc.window = app->window;
        androidDesc.sType = wgpu::SType::SurfaceDescriptorFromAndroidNativeWindow;
    }
    void Init();
    void Frame();
    void GameLoop();
};

#endif  // APP_SRC_MAIN_CPP_RENDERER_H_
