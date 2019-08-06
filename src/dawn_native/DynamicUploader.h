// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_DYNAMICUPLOADER_H_
#define DAWNNATIVE_DYNAMICUPLOADER_H_

#include "dawn_native/Forward.h"
#include "dawn_native/RingBufferAllocator.h"

// DynamicUploader is the front-end implementation used to manage multiple ring buffers for upload
// usage.
namespace dawn_native {

    struct UploadHandle {
        uint8_t* mappedBuffer = nullptr;
        size_t startOffset = 0;
        StagingBufferBase* stagingBuffer = nullptr;
    };

    class DynamicUploader {
      public:
        DynamicUploader(DeviceBase* device);
        ~DynamicUploader() = default;

        // We add functions to Create/Release StagingBuffers to the DynamicUploader as there's
        // currently no place to track the allocated staging buffers such that they're freed after
        // pending coommands are finished. This should be changed when better resource allocation is
        // implemented.
        ResultOrError<std::unique_ptr<StagingBufferBase>> CreateStagingBuffer(size_t size);
        void ReleaseStagingBuffer(std::unique_ptr<StagingBufferBase> stagingBuffer);

        ResultOrError<UploadHandle> Allocate(uint32_t requiredSize, uint32_t alignment);
        void Tick(Serial lastCompletedSerial);

        RingBufferAllocator* GetLargestBuffer();

        MaybeError CreateAndAppendBuffer(size_t size = kBaseUploadBufferSize);

        bool IsEmpty() const;

      private:
        // TODO(bryan.bernhart@intel.com): Figure out this value.
        static constexpr size_t kBaseUploadBufferSize = 64000;

        std::vector<std::unique_ptr<RingBufferAllocator>> mRingBuffers;
        SerialQueue<std::unique_ptr<StagingBufferBase>> mReleasedStagingBuffers;
        DeviceBase* mDevice;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_DYNAMICUPLOADER_H_
