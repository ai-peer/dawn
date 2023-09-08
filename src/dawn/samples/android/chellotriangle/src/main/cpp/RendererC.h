//
// Created by nexa on 8/18/23.
//
#include <dawn/webgpu.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include "Log.h"

#ifndef APP_SRC_MAIN_CPP2_RENDERER_H_
#define APP_SRC_MAIN_CPP2_RENDERER_H_

class RendererC {
  public:
    android_app* mApp;
    WGPUInstanceDescriptor desc;
    WGPUInstance instance;
    ANativeWindow* nativeWindow;
    WGPURequestAdapterOptions adapterOpts;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUDeviceDescriptor deviceDesc;
    WGPUSurfaceDescriptorFromAndroidNativeWindow androidDesc;
    WGPUSwapChain swapChain;
    WGPURenderPipeline pipeline;
    WGPUQueue queue;
    bool deviceInitialised;

    RendererC(android_app* app) {
        deviceInitialised = false;
        mApp = app;
        desc = {};
        desc.nextInChain = nullptr;
        instance = wgpuCreateInstance(&desc);

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

    ~RendererC() {
        // TODO:Ensure all resources get released.
    }

    void initRenderer();
    void GameLoop();
};

#endif  // APP_SRC_MAIN_CPP2_RENDERER_H_
