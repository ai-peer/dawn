//
// Created by nexa on 8/18/23.
//

#include <webgpu/webgpu_cpp.h>
#include <cassert>
#include <vector>
#include "Log.h"

/**
 * Utility function to get a WebGPU adapter, so that
 */
wgpu::Adapter requestAdapter(wgpu::Instance instance, wgpu::RequestAdapterOptions const* options) {
    // A simple structure holding the local information shared with the
    // onAdapterRequestEnded callback.
    struct UserData {
        wgpu::Adapter adapter = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                    char const* message, void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = wgpu::Adapter::Acquire(adapter);
        } else {
            LOGI("Could not get WebGPU adapter: %s", message);
        }
        userData.requestEnded = true;
    };

    // Call to the WebGPU request adapter procedure
    instance.RequestAdapter(options, onAdapterRequestEnded, (void*)&userData);

    assert(userData.requestEnded);
    return userData.adapter;
}

/**
 * Utility function to get a WebGPU device
 */
wgpu::Device requestDevice(wgpu::Adapter adapter, wgpu::DeviceDescriptor const* descriptor) {
    struct UserData {
        wgpu::Device device = nullptr;
        bool requestEnded = false;
    };
    UserData userData;

    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice cdevice,
                                   char const* message, void* pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestDeviceStatus_Success) {
            userData.device = wgpu::Device::Acquire(cdevice);
        } else {
            LOGI("Could not get WebGPU device: ");
        }
        userData.requestEnded = true;
    };

    adapter.RequestDevice(descriptor, onDeviceRequestEnded, (void*)&userData);

    assert(userData.requestEnded);

    return userData.device;
}