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

#include "dawn_native/metal/QueueMTL.h"

#include "common/Math.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/metal/CommandBufferMTL.h"
#include "dawn_native/metal/DeviceMTL.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

namespace dawn_native {
    namespace {
        void* MovePointerByBytes(void* ptr, int inc) {
            uint8_t* castPtr = static_cast<uint8_t*>(ptr);
            return static_cast<void*>(castPtr + inc);
        }
        const void* MovePointerByBytes(const void* ptr, int inc) {
            const uint8_t* castPtr = static_cast<const uint8_t*>(ptr);
            return static_cast<const void*>(castPtr + inc);
        }
    }

    namespace metal {

        Queue::Queue(Device* device) : QueueBase(device) {
        }

        MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
            Device* device = ToBackend(GetDevice());
            device->Tick();
            CommandRecordingContext* commandContext = device->GetPendingCommandContext();

            TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording,
                               "CommandBufferMTL::FillCommands");
            for (uint32_t i = 0; i < commandCount; ++i) {
                DAWN_TRY(ToBackend(commands[i])->FillCommands(commandContext));
            }
            TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording,
                             "CommandBufferMTL::FillCommands");

            device->SubmitPendingCommandBuffer();
            return {};
        }

        ResultOrError<UploadHandle> Queue::UploadTextureDataAligningBytesPerRow(
            const void* data,
            size_t dataSize,
            uint32_t alignedBytesPerRow,
            const TextureDataLayout* dataLayout,
            const Format& textureFormat,
            const Extent3D* writeSize) {
            DeviceBase* device = GetDevice();

            uint32_t newDataSize;
            ComputeRequiredBytesInCopy(textureFormat, *writeSize, alignedBytesPerRow,
                                       dataLayout->rowsPerImage, &newDataSize);

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                              newDataSize, device->GetPendingCommandSerial()));
            ASSERT(uploadHandle.mappedBuffer != nullptr);

            uint64_t bufferOffset = 0;
            uint64_t dataOffset = dataLayout->offset;

            ASSERT(writeSize->depth >= 1);
            for (uint32_t d = 0; d < writeSize->depth - 1; ++d) {
                for (uint32_t h = 0; h < dataLayout->rowsPerImage; ++h) {
                    memcpy(MovePointerByBytes(uploadHandle.mappedBuffer, bufferOffset),
                           MovePointerByBytes(data, dataOffset), dataLayout->bytesPerRow);
                    bufferOffset += alignedBytesPerRow;
                    dataOffset += dataLayout->bytesPerRow;
                }
            }

            ASSERT(writeSize->height >= 1);
            for (uint32_t h = 0; h < writeSize->height - 1; ++h) {
                memcpy(MovePointerByBytes(uploadHandle.mappedBuffer, bufferOffset),
                       MovePointerByBytes(data, dataOffset), dataLayout->bytesPerRow);
                bufferOffset += alignedBytesPerRow;
                dataOffset += dataLayout->bytesPerRow;
            }

            memcpy(MovePointerByBytes(uploadHandle.mappedBuffer, bufferOffset),
                   MovePointerByBytes(data, dataOffset),
                   writeSize->width * textureFormat.blockByteSize);

            return uploadHandle;
        }

        // We don't write from the CPU to the texture directly which can be done in Metal using the
        // replaceRegion function, because the function requires a non-private storage mode and Dawn
        // sets the private storage mode by default for all textures except IOSurfaces on macOS.
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
            DAWN_TRY_ASSIGN(uploadHandle, UploadTextureDataAligningBytesPerRow(
                                              data, dataSize, alignedBytesPerRow, dataLayout,
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

    }
}  // namespace dawn_native::metal
