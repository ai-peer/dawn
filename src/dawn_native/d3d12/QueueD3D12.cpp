// Copyright 2017 The Dawn Authors
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

#include "dawn_native/d3d12/QueueD3D12.h"

#include "common/Math.h"
#include "dawn_native/Buffer.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Commands.h"
#include "dawn_native/DynamicUploader.h"
#include "dawn_native/d3d12/CommandBufferD3D12.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

namespace dawn_native { namespace d3d12 {

    namespace {
        ResultOrError<UploadHandle> UploadTextureDataAligningBytesPerRow(
            DeviceBase* device,
            const void* data,
            size_t dataSize,
            uint32_t alignedBytesPerRow,
            uint32_t optimallyAlignedBytesPerRow,
            uint32_t alignedRowsPerImage,
            const TextureDataLayout* dataLayout,
            const Format& textureFormat,
            const Extent3D* writeSize) {
            uint32_t newDataSize = ComputeRequiredBytesInCopy(
                textureFormat, *writeSize, optimallyAlignedBytesPerRow, alignedRowsPerImage);

            UploadHandle uploadHandle;
            DAWN_TRY_ASSIGN(uploadHandle, device->GetDynamicUploader()->Allocate(
                                              newDataSize, device->GetPendingCommandSerial()));
            ASSERT(uploadHandle.mappedBuffer != nullptr);

            uint8_t* dstPointer = static_cast<uint8_t*>(uploadHandle.mappedBuffer);
            const uint8_t* srcPointer = static_cast<const uint8_t*>(data);
            srcPointer += dataLayout->offset;

            uint32_t alignedRowsPerImageInBlock = alignedRowsPerImage / textureFormat.blockHeight;
            uint32_t dataRowsPerImageInBlock = dataLayout->rowsPerImage / textureFormat.blockHeight;
            if (dataRowsPerImageInBlock == 0) {
                dataRowsPerImageInBlock = writeSize->height / textureFormat.blockHeight;
            }

            ASSERT(dataRowsPerImageInBlock >= alignedRowsPerImageInBlock);
            uint64_t imageAdditionalStride =
                dataLayout->bytesPerRow * (dataRowsPerImageInBlock - alignedRowsPerImageInBlock);

            CopyTextureData(dstPointer, srcPointer, writeSize->depth, alignedRowsPerImageInBlock,
                            imageAdditionalStride, alignedBytesPerRow, optimallyAlignedBytesPerRow,
                            dataLayout->bytesPerRow);

            return uploadHandle;
        }
    }  // namespace

    Queue::Queue(Device* device) : QueueBase(device) {
    }

    MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
        Device* device = ToBackend(GetDevice());

        device->Tick();

        CommandRecordingContext* commandContext;
        DAWN_TRY_ASSIGN(commandContext, device->GetPendingCommandContext());

        TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording,
                           "CommandBufferD3D12::RecordCommands");
        for (uint32_t i = 0; i < commandCount; ++i) {
            DAWN_TRY(ToBackend(commands[i])->RecordCommands(commandContext));
        }
        TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording,
                         "CommandBufferD3D12::RecordCommands");

        DAWN_TRY(device->ExecutePendingCommandContext());

        DAWN_TRY(device->NextSerial());
        return {};
    }

    MaybeError Queue::WriteTextureImpl(const TextureCopyView* destination,
                                       const void* data,
                                       size_t dataSize,
                                       const TextureDataLayout* dataLayout,
                                       const Extent3D* writeSize) {
        uint32_t blockSize = destination->texture->GetFormat().blockByteSize;
        uint32_t blockWidth = destination->texture->GetFormat().blockWidth;
        // We are only copying the part of the data that will appear in the texture.
        // Note that validating texture copy range ensures that writeSize->width and
        // writeSize->height are multiples of blockWidth and blockHeight respectively.
        uint32_t alignedBytesPerRow = (writeSize->width) / blockWidth * blockSize;
        uint32_t alignedRowsPerImage = writeSize->height;
        uint32_t optimallyAlignedBytesPerRow =
            Align(alignedBytesPerRow, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

        UploadHandle uploadHandle;
        DAWN_TRY_ASSIGN(
            uploadHandle,
            UploadTextureDataAligningBytesPerRow(
                GetDevice(), data, dataSize, alignedBytesPerRow, optimallyAlignedBytesPerRow,
                alignedRowsPerImage, dataLayout, destination->texture->GetFormat(), writeSize));

        TextureDataLayout passDataLayout = *dataLayout;
        passDataLayout.offset = uploadHandle.startOffset;
        passDataLayout.bytesPerRow = optimallyAlignedBytesPerRow;
        passDataLayout.rowsPerImage = alignedRowsPerImage;

        TextureCopy textureCopy;
        textureCopy.texture = destination->texture;
        textureCopy.mipLevel = destination->mipLevel;
        textureCopy.origin = destination->origin;

        return ToBackend(GetDevice())
            ->CopyFromStagingToTexture(uploadHandle.stagingBuffer, passDataLayout, &textureCopy,
                                       *writeSize);
    }

}}  // namespace dawn_native::d3d12
