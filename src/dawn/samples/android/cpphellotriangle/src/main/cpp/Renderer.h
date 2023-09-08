//
// Created by nexa on 8/18/23.
//
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <webgpu/webgpu_cpp.h>

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

    Renderer(android_app* app) {

        deviceInitialised = false;
        androidDesc.window = app->window;
        androidDesc.sType = wgpu::SType::SurfaceDescriptorFromAndroidNativeWindow;
    }

    void Init();
    void Frame();
    void GameLoop();
};

#endif  // APP_SRC_MAIN_CPP_RENDERER_H_
