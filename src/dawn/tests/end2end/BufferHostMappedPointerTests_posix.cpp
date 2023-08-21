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

#include <sys/mman.h>
#include <unistd.h>

#include "dawn/tests/end2end/BufferHostMappedPointerTests.h"

namespace dawn {
namespace {

class MMapBackend : public BufferHostMappedPointerTestBackend {
  public:
    static BufferHostMappedPointerTestBackend* GetInstance() {
        static MMapBackend backend;
        return &backend;
    }

    const char* Name() const override { return "mmap"; }

    std::pair<wgpu::Buffer, void*> CreateHostMappedBuffer(
        wgpu::Device device,
        wgpu::BufferUsage usage,
        size_t size,
        std::function<void(void*)> Populate) override {
        // Create a temporary file.
        char filename[] = "tmpXXXXXX";
        int fd = mkstemp(filename);
        EXPECT_GT(fd, -1);

        unlink(filename);

        // Write the initial data.
        std::vector<char> initialData(size);
        Populate(initialData.data());
        EXPECT_EQ(write(fd, initialData.data(), size), (signed)size);

        // Memory map the file.
        void* ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

        wgpu::BufferHostMappedPointer hostMappedDesc;
        hostMappedDesc.pointer = ptr;
        hostMappedDesc.disposeCallback = [](void* userdata) {
            auto* data = reinterpret_cast<std::tuple<int, void*, size_t>*>(userdata);
            munmap(std::get<1>(*data), std::get<2>(*data));
            close(std::get<0>(*data));
            delete data;
        };
        hostMappedDesc.userdata = new std::tuple<int, void*, size_t>(fd, ptr, size);

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.usage = usage;
        bufferDesc.size = size;
        bufferDesc.nextInChain = &hostMappedDesc;
        return std::make_pair(device.CreateBuffer(&bufferDesc), hostMappedDesc.pointer);
    }
};

DAWN_INSTANTIATE_PREFIXED_TEST_P(POSIX,
                                 BufferHostMappedPointerTests,
                                 {MetalBackend(), VulkanBackend()},
                                 {MMapBackend::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
