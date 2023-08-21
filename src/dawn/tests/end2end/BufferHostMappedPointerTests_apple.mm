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

#include <mach/mach.h>

#include "dawn/tests/end2end/BufferHostMappedPointerTests.h"

namespace dawn {
namespace {

class VMBackend : public BufferHostMappedPointerTestBackend {
  public:
    static BufferHostMappedPointerTestBackend* GetInstance() {
        static VMBackend backend;
        return &backend;
    }

    const char* Name() const override { return "vm_allocate"; }

    std::pair<wgpu::Buffer, void*> CreateHostMappedBuffer(
        wgpu::Device device,
        wgpu::BufferUsage usage,
        size_t size,
        std::function<void(void*)> Populate) override {
        vm_address_t addr = 0;
        EXPECT_EQ(vm_allocate(mach_task_self(), &addr, size, /* anywhere */ true), KERN_SUCCESS);

        void* ptr = reinterpret_cast<void*>(addr);
        Populate(ptr);

        wgpu::BufferHostMappedPointer hostMappedDesc;
        hostMappedDesc.pointer = ptr;
        hostMappedDesc.disposeCallback = [](void* userdata) {
            auto* data = reinterpret_cast<std::pair<vm_address_t, size_t>*>(userdata);
            vm_deallocate(mach_task_self(), data->first, data->second);
            delete data;
        };
        hostMappedDesc.userdata = new std::pair<vm_address_t, size_t>(addr, size);

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.usage = usage;
        bufferDesc.size = size;
        bufferDesc.nextInChain = &hostMappedDesc;
        return std::make_pair(device.CreateBuffer(&bufferDesc), hostMappedDesc.pointer);
    }
};

DAWN_INSTANTIATE_PREFIXED_TEST_P(Apple,
                                 BufferHostMappedPointerTests,
                                 {MetalBackend()},
                                 {VMBackend::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
