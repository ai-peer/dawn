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

#include "dawn_native/metal/CommandBufferMTL.h"
#include "dawn_native/metal/DeviceMTL.h"
#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/tracing/TraceEvent.h"

namespace dawn_native { namespace metal {

    Queue::Queue(Device* device) : QueueBase(device) {
    }

    MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
        Device* device = ToBackend(GetDevice());
        device->Tick();
        CommandRecordingContext* commandContext = device->GetPendingCommandContext();

        TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferMTL::FillCommands");
        for (uint32_t i = 0; i < commandCount; ++i) {
            DAWN_TRY(ToBackend(commands[i])->FillCommands(commandContext));
        }
        TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferMTL::FillCommands");

        device->SubmitPendingCommandBuffer();
        return {};
    }

    MaybeError Queue::WriteTextureImpl(const TextureCopyView* destination,
                                       const void* data,
                                       size_t dataSize,
                                       const TextureDataLayout* dataLayout,
                                       const Extent3D* writeSize) {
        TextureCopy textureCopy;
        textureCopy.texture = destination->texture;
        textureCopy.mipLevel = destination->mipLevel;
        textureCopy.origin = destination->origin;

        Texture* texture = ToBackend(destination->texture);
        EnsureDestinationTextureInitialized(texture, textureCopy, *writeSize);

        const Extent3D virtualSizeAtLevel = texture->GetMipLevelVirtualSize(destination->mipLevel);

        TextureBufferCopySplit splitCopies = ComputeTextureBufferCopySplit(
            texture->GetDimension(), textureCopy.origin, *writeSize, texture->GetFormat(),
            virtualSizeAtLevel, dataSize, dataLayout->offset, dataLayout->bytesPerRow,
            dataLayout->rowsPerImage);

        for (uint32_t i = 0; i < splitCopies.count; ++i) {
            const TextureBufferCopySplit::CopyInfo& copyInfo = splitCopies.copies[i];

            const uint32_t copyBaseLayer = copyInfo.textureOrigin.z;
            const uint32_t copyLayerCount = copyInfo.copyExtent.depth;

            uint64_t bufferOffset = copyInfo.bufferOffset;
            for (uint32_t copyLayer = copyBaseLayer; copyLayer < copyBaseLayer + copyLayerCount;
                 ++copyLayer) {
                //const char* newData = static_cast<const char*>(data) + bufferOffset;

            data = malloc(10000000);

            /*printf("Will be writing to the texture: \n");
            printf("region = %d %d %d %d\n", copyInfo.textureOrigin.x, copyInfo.textureOrigin.y,
                                             copyInfo.copyExtent.width, copyInfo.copyExtent.height);
            printf("mipmapLevel = %d\n", destination->mipLevel);
            printf("slice = %d\n", copyLayer);
            printf("bytesPerRow = %d\n", (int)copyInfo.bytesPerRow);
            printf("bytesPerImage = %d\n", (int)copyInfo.bytesPerImage);

            printf("dataSize = %d\n", (int)dataSize);*/


                [texture->GetMTLTexture()
                    replaceRegion:MTLRegionMake2D(
                                      copyInfo.textureOrigin.x, copyInfo.textureOrigin.y,
                                      copyInfo.copyExtent.width, copyInfo.copyExtent.height)
                      mipmapLevel:destination->mipLevel
                            slice:copyLayer
                        withBytes:data
                      bytesPerRow:copyInfo.bytesPerRow
                    bytesPerImage:copyInfo.bytesPerImage];

                bufferOffset += copyInfo.bytesPerImage;
            }
        }

        return {};
    }
}}  // namespace dawn_native::metal
