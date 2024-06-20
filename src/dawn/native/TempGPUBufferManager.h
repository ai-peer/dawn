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

#include <absl/container/flat_hash_map.h>
#include <memory>
#include <optional>
#include <vector>

#include "dawn/common/Ref.h"
#include "dawn/common/SerialQueue.h"
#include "dawn/native/Error.h"
#include "dawn/native/Forward.h"
#include "dawn/native/IntegerTypes.h"

namespace dawn::native {

class BufferBase;
class DeviceBase;

// This buffer manager will allocate and reuse temp GPU buffers as much possible.
// If the buffer is small enough, often a power of two allocation will be used. It is to reduce the
// number of different allocation sizes need to be tracked. track. Otherwise, if the buffer is
// large, exact size will be used.
class TempGPUBufferManager {
  public:
    // Note: device must outlive this manager.
    explicit TempGPUBufferManager(DeviceBase* device, wgpu::BufferUsage bufferUsage);
    ~TempGPUBufferManager();

    ResultOrError<BufferBase*> Allocate(uint64_t allocationSize, ExecutionSerial useInSerial);

    void Deallocate(ExecutionSerial completedSerial);

  private:
    BufferBase* TrackBuffer(Ref<BufferBase> buffer, ExecutionSerial useInSerial);
    ResultOrError<BufferBase*> DoAllocateBuffer(uint64_t size, ExecutionSerial useInSerial);

    raw_ptr<DeviceBase> mDevice;
    const wgpu::BufferUsage mBufferUsage;

    using BufferSerialQueue = SerialQueue<ExecutionSerial, Ref<BufferBase>>;
    BufferSerialQueue mInflightAllocations;

    using BufferFreeList = absl::flat_hash_map<uint64_t, BufferSerialQueue>;
    BufferFreeList mBuffersFreeList;

    uint64_t mNextAllocationID = 0;
};

}  // namespace dawn::native

#endif
