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
        ResultOrError<UploadHandle> UploadTextureDataAligningBytesPerRow(
            DeviceBase* device,
            const void* data,
            size_t dataSize,
            uint32_t alignedBytesPerRow,
            uint32_t alignedRowsPerImage,
            const TextureDataLayout* dataLayout,
            const Format& textureFormat,
            const Extent3D* writeSize) {
            uint32_t newDataSize = ComputeRequiredBytesInCopy(
                textureFormat, *writeSize, alignedBytesPerRow, alignedRowsPerImage);

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                              newDataSize, device->GetPendingCommandSerial()));
            ASSERT(uploadHandle.mappedBuffer != nullptr);

            uint8_t* dstPointer = static_cast<uint8_t*>(uploadHandle.mappedBuffer);
            const uint8_t* srcPointer = static_cast<const uint8_t*>(data);
            srcPointer += dataLayout->offset;

            ASSERT(dataLayout->rowsPerImage >= alignedRowsPerImage);
            for (uint32_t d = 0; d < writeSize->depth; ++d) {
                for (uint32_t h = 0; h < alignedRowsPerImage; ++h) {
                    memcpy(dstPointer, srcPointer, alignedBytesPerRow);
                    dstPointer += alignedBytesPerRow;
                    srcPointer += dataLayout->bytesPerRow;
                }
                if (d + 1 < writeSize->depth) {
                    srcPointer +=
                        dataLayout->bytesPerRow * (dataLayout->rowsPerImage - alignedRowsPerImage);
                }
            }

            return uploadHandle;
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

        // We don't write from the CPU to the texture directly which can be done in Metal using the
        // replaceRegion function, because the function requires a non-private storage mode and Dawn
        // sets the private storage mode by default for all textures except IOSurfaces on macOS.
        MaybeError Queue::WriteTextureImpl(const TextureCopyView* destination,
                                           const void* data,
                                           size_t dataSize,
                                           const TextureDataLayout* dataLayout,
                                           const Extent3D* writeSize) {
            uint32_t blockSize = destination->texture->GetFormat().blockByteSize;
            uint32_t blockWidth = destination->texture->GetFormat().blockWidth;
            uint32_t blockHeight = destination->texture->GetFormat().blockHeight;
            // We are only copying the part of the data that will appear in the texture.
            uint32_t alignedBytesPerRow = (writeSize->width) / blockWidth * blockSize;
            uint32_t alignedRowsPerImage = (writeSize->height) / blockHeight * blockHeight;

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle, UploadTextureDataAligningBytesPerRow(
                                              GetDevice(), data, dataSize, alignedBytesPerRow,
                                              alignedRowsPerImage, dataLayout,
                                              destination->texture->GetFormat(), writeSize));

            TextureDataLayout passDataLayout = *dataLayout;
            passDataLayout.offset = uploadHandle.startOffset;
            passDataLayout.bytesPerRow = alignedBytesPerRow;
            passDataLayout.rowsPerImage = alignedRowsPerImage;

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
