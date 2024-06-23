// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_DAWN_NATIVE_TEMPGPUBUFFERMANAGER_H_
#define SRC_DAWN_NATIVE_TEMPGPUBUFFERMANAGER_H_

#include <memory>

#include "dawn/common/Ref.h"
#include "dawn/common/SerialQueue.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/BuddyMemoryAllocator.h"
#include "dawn/native/ResourceMemoryAllocation.h"

namespace dawn::native {

class BufferBase;
class DeviceBase;

// We use suballocations for allocating temporary GPU buffers.
class TempGPUBufferManager {
  public:
    struct Allocation {
        raw_ptr<BufferBase> buffer = nullptr;
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    // Note: device must outlive this manager.
    explicit TempGPUBufferManager(DeviceBase* device, wgpu::BufferUsage bufferUsage);
    ~TempGPUBufferManager();

    ResultOrError<Allocation> Allocate(uint64_t allocationSize,
                                       ExecutionSerial useInSerial,
                                       uint64_t offsetAlignment);

    void Deallocate(ExecutionSerial completedSerial);

  private:
    class Allocator;

    raw_ptr<DeviceBase> mDevice;

    std::unique_ptr<Allocator> mAllocator;
    BuddyMemoryAllocator mBuddySubAllocator;
    SerialQueue<ExecutionSerial, ResourceMemoryAllocation> mInflightSubAllocations;
    SerialQueue<ExecutionSerial, Ref<BufferBase>> mInflightAllocations;
};

}  // namespace dawn::native

#endif
