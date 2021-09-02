// Copyright 2021 The Dawn Authors
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

#include "dawn_native/ValidationScratchBuffer.h"

#include "dawn_native/Device.h"

namespace dawn_native {

    ValidationScratchBuffer::ValidationScratchBuffer(DeviceBase* device) : mDevice(device) {
    }

    ValidationScratchBuffer::~ValidationScratchBuffer() = default;

    MaybeError ValidationScratchBuffer::Reset(uint64_t capacity) {
        if (!mBuffer.Get() || mBuffer->GetSize() < capacity) {
            BufferDescriptor descriptor;
            descriptor.size = capacity;
            descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Indirect |
                               wgpu::BufferUsage::Storage;
            DAWN_TRY_ASSIGN(mBuffer, mDevice->CreateBuffer(&descriptor));
        }
        mNumOccupiedBytes = 0;
        return {};
    }

    void ValidationScratchBuffer::Release() {
        mBuffer = nullptr;
    }

    uint64_t ValidationScratchBuffer::Claim(uint64_t numBytes) {
        ASSERT(mBuffer.Get() != nullptr);
        ASSERT(numBytes <= mBuffer->GetSize());
        ASSERT(mBuffer->GetSize() - numBytes >= mNumOccupiedBytes);
        const uint64_t offset = mNumOccupiedBytes;
        mNumOccupiedBytes += numBytes;
        return offset;
    }

}  // namespace dawn_native