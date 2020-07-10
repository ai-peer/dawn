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

#include "dawn_native/vulkan/QueueVk.h"

#include "dawn_native/DynamicUploader.h"
#include "dawn_native/vulkan/CommandBufferVk.h"
#include "dawn_native/vulkan/CommandRecordingContext.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

namespace dawn_native { namespace vulkan {

    // static
    Queue* Queue::Create(Device* device) {
        return new Queue(device);
    }

    Queue::~Queue() {
    }

    MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
        Device* device = ToBackend(GetDevice());

        device->Tick();

        TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording,
                           "CommandBufferVk::RecordCommands");
        CommandRecordingContext* recordingContext = device->GetPendingRecordingContext();
        for (uint32_t i = 0; i < commandCount; ++i) {
            DAWN_TRY(ToBackend(commands[i])->RecordCommands(recordingContext));
        }
        TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferVk::RecordCommands");

        DAWN_TRY(device->SubmitPendingCommands());

        return {};
    }

    MaybeError Queue::WriteTextureImpl(const TextureCopyView* destination,
                                       const void* data,
                                       size_t dataSize,
                                       const TextureDataLayout* dataLayout,
                                       const Extent3D* writeSize) {
        if (dataSize == 0) {
            return {};
        }

        if (dataLayout->bytesPerRow % destination->texture->GetFormat().blockByteSize != 0) {
            return DAWN_VALIDATION_ERROR(
                "In Vulkan bytesPerRow must be a multiple of texel block size");
        }

        DeviceBase* device = GetDevice();

        UploadHandle uploadHandle;
        DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                          dataSize, device->GetPendingCommandSerial()));
        ASSERT(uploadHandle.mappedBuffer != nullptr);

        memcpy(uploadHandle.mappedBuffer, data, dataSize);

        uint32_t defaultedRowsPerImage = dataLayout->rowsPerImage;
        if (defaultedRowsPerImage == 0) {
            defaultedRowsPerImage = writeSize->height;
        }

        TextureDataLayout passDataLayout;
        passDataLayout.offset = dataLayout->offset + uploadHandle.startOffset;
        passDataLayout.rowsPerImage = defaultedRowsPerImage;
        passDataLayout.bytesPerRow = dataLayout->bytesPerRow;

        TextureCopy textureCopy;
        textureCopy.texture = destination->texture;
        textureCopy.mipLevel = destination->mipLevel;
        textureCopy.origin = destination->origin;

        return ToBackend(device)->CopyFromStagingToTexture(uploadHandle.stagingBuffer,
                                                           passDataLayout, textureCopy, *writeSize);
    }

}}  // namespace dawn_native::vulkan
