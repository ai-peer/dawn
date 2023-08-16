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

#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <cstdio>

int main() {
    dawnProcSetProcs(&dawn::native::GetProcs());

    WGPUInstance instance = wgpuCreateInstance(nullptr);
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.backendType = WGPUBackendType_OpenGL;
    adapterOpts.compatibilityMode = true;

    WGPUAdapter adapter;
    wgpuInstanceRequestAdapter(
        instance, &adapterOpts,
        [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const* message,
           void* userdata) {
            if (message) {
                fprintf(stderr, "%s\n", message);
            }
            *reinterpret_cast<WGPUAdapter*>(userdata) = adapter;
        },
        &adapter);
    if (!adapter) {
        fprintf(stderr, "No adapters.\n");
        return 1;
    }

    WGPUDevice device;
    wgpuAdapterRequestDevice(
        adapter, nullptr,
        [](WGPURequestDeviceStatus status, WGPUDevice device, char const* message, void* userdata) {
            if (message) {
                fprintf(stderr, "%s\n", message);
            }
            *reinterpret_cast<WGPUDevice*>(userdata) = device;
        },
        &device);
    if (!device) {
        fprintf(stderr, "Failed to create device.\n");
        return 1;
    }
    printf("Created device %p.\n", (void*)device);
    return 0;
}
