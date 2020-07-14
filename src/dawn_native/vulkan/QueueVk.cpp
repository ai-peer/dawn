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

#include "common/Math.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/vulkan/CommandBufferVk.h"
#include "dawn_native/vulkan/CommandRecordingContext.h"
#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

namespace dawn_native {

    namespace {
        ResultOrError<UploadHandle> UploadTextureDataAligningBytesPerRow(
            DeviceBase* device,
            const void* data,
            size_t dataSize,
            uint32_t alignedBytesPerRow,
            const TextureDataLayout* dataLayout,
            const Format& textureFormat,
            const Extent3D* writeSize) {
            uint32_t newDataSize;
            ComputeRequiredBytesInCopy(textureFormat, *writeSize, alignedBytesPerRow,
                                       dataLayout->rowsPerImage, &newDataSize);

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                              newDataSize, device->GetPendingCommandSerial()));
            ASSERT(uploadHandle.mappedBuffer != nullptr);

            uint8_t* dstPointer = static_cast<uint8_t*>(uploadHandle.mappedBuffer);
            const uint8_t* srcPointer = static_cast<const uint8_t*>(data);
            srcPointer += dataLayout->offset;

            ASSERT(writeSize->depth >= 1);
            for (uint32_t d = 0; d < writeSize->depth - 1; ++d) {
                for (uint32_t h = 0; h < dataLayout->rowsPerImage; ++h) {
                    memcpy(dstPointer, srcPointer, alignedBytesPerRow);
                    dstPointer += alignedBytesPerRow;
                    srcPointer += dataLayout->bytesPerRow;
                }
            }

            ASSERT(writeSize->height >= 1);
            for (uint32_t h = 0; h < writeSize->height - 1; ++h) {
                memcpy(dstPointer, srcPointer, alignedBytesPerRow);
                dstPointer += alignedBytesPerRow;
                srcPointer += dataLayout->bytesPerRow;
            }

            memcpy(dstPointer, srcPointer, writeSize->width * textureFormat.blockByteSize);

            return uploadHandle;
        }
    }  // namespace

    namespace vulkan {

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
            TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording,
                             "CommandBufferVk::RecordCommands");

            DAWN_TRY(device->SubmitPendingCommands());

            return {};
        }

        MaybeError Queue::WriteTextureImpl(const TextureCopyView* destination,
                                           const void* data,
                                           size_t dataSize,
                                           const TextureDataLayout* dataLayout,
                                           const Extent3D* writeSize) {
            uint32_t blockSize = destination->texture->GetFormat().blockByteSize;
            // We are not copying the last bytes of data in each row when bytesPerRow is not
            // blockSize aligned.
            uint32_t alignedBytesPerRow = (dataLayout->bytesPerRow) / blockSize * blockSize;

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle,
                            UploadTextureDataAligningBytesPerRow(
                                GetDevice(), data, dataSize, alignedBytesPerRow, dataLayout,
                                destination->texture->GetFormat(), writeSize));

            TextureDataLayout passDataLayout = *dataLayout;
            passDataLayout.offset = uploadHandle.startOffset;
            passDataLayout.bytesPerRow = alignedBytesPerRow;
            if (passDataLayout.rowsPerImage == 0) {
                passDataLayout.rowsPerImage = writeSize->height;
            }

            TextureCopy textureCopy;
            textureCopy.texture = destination->texture;
            textureCopy.mipLevel = destination->mipLevel;
            textureCopy.origin = destination->origin;

            return ToBackend(GetDevice())
                ->CopyFromStagingToTexture(uploadHandle.stagingBuffer, passDataLayout, &textureCopy,
                                           *writeSize);
        }

    }  // namespace vulkan
}  // namespace dawn_native