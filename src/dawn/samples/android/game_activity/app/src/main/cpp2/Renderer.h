//
// Created by nexa on 8/18/23.
//
#include <webgpu/webgpu_cpp.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "Log.h"

#ifndef APP_SRC_MAIN_CPP_RENDERER_H_
#define APP_SRC_MAIN_CPP_RENDERER_H_

class Renderer {
 public:
    android_app *mApp;
    wgpu::Instance instance;
    wgpu::InstanceDescriptor desc;
    wgpu::RequestAdapterOptions adapterOpts;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::DeviceDescriptor deviceDesc;
    wgpu::SurfaceDescriptorFromAndroidNativeWindow androidDesc;
    wgpu::SwapChain swapChain;
    wgpu::RenderPipeline pipeline;
    wgpu::Queue queue;
    bool deviceInitialised;

    Renderer(android_app* app) {
    deviceInitialised = false;
    mApp = app;
    desc = {};
    desc.nextInChain = nullptr;
    instance = wgpu::CreateInstance(&desc);
    if (!instance) {
        LOGE("Could not initialize WebGPU!");
        return;
    }

    adapterOpts = {};
    adapterOpts.nextInChain = nullptr;

    androidDesc = {};
    deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    }

    ~Renderer() {

   //TODO: Find how to release resources
    }

    void initRenderer();
    void GameLoop();
};

#endif //APP_SRC_MAIN_CPP_RENDERER_H_
