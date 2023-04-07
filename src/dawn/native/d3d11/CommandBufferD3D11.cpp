// Copyright 2023 The Dawn Authors
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

#include "dawn/native/d3d11/CommandBufferD3D11.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include "dawn/common/WindowsUtils.h"
#include "dawn/native/BindGroup.h"
#include "dawn/native/BindGroupTracker.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/Commands.h"
#include "dawn/native/ExternalTexture.h"
#include "dawn/native/RenderBundle.h"
#include "dawn/native/VertexFormat.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/ComputePipelineD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/Forward.h"
#include "dawn/native/d3d11/PipelineLayoutD3D11.h"
#include "dawn/native/d3d11/RenderPipelineD3D11.h"
#include "dawn/native/d3d11/SamplerD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {

// Create CommandBuffer
Ref<CommandBuffer> CommandBuffer::Create(CommandEncoder* encoder,
                                         const CommandBufferDescriptor* descriptor) {
    Ref<CommandBuffer> commandBuffer = AcquireRef(new CommandBuffer(encoder, descriptor));
    return commandBuffer;
}

MaybeError CommandBuffer::Execute() {
    CommandRecordingContext* commandContext = nullptr;
    DAWN_TRY_ASSIGN(commandContext, ToBackend(GetDevice())->GetPendingCommandContext());

    ID3D11DeviceContext1* d3d11DeviceContext1 = commandContext->GetD3D11DeviceContext1();

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::BeginComputePass: {
                mCommands.NextCommand<BeginComputePassCmd>();
                DAWN_TRY(ExecuteComputePass(commandContext));
                break;
            }

            case Command::BeginRenderPass: {
                auto* cmd = mCommands.NextCommand<BeginRenderPassCmd>();
                DAWN_TRY(ExecuteRenderPass(cmd, commandContext));
                break;
            }

            case Command::CopyBufferToBuffer: {
                CopyBufferToBufferCmd* copy = mCommands.NextCommand<CopyBufferToBufferCmd>();
                if (copy->size == 0) {
                    // Skip no-op copies.
                    break;
                }

                Buffer* source = ToBackend(copy->source.Get());
                Buffer* destination = ToBackend(copy->destination.Get());

                // CopyFromBuffer() will ensure the source and destination buffers are initialized.
                DAWN_TRY(destination->CopyFromBuffer(commandContext, copy->destinationOffset,
                                                     copy->size, source, copy->sourceOffset));
                break;
            }

            case Command::CopyBufferToTexture: {
                CopyBufferToTextureCmd* copy = mCommands.NextCommand<CopyBufferToTextureCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }

                Buffer* buffer = ToBackend(copy->source.buffer.Get());
                Buffer::ScopedMap scopedMap(buffer, "CopyBufferToTexture");
                DAWN_TRY(scopedMap.AcquireResult());
                DAWN_TRY(buffer->EnsureDataInitialized(commandContext));

                Texture* texture = ToBackend(copy->destination.texture.Get());
                SubresourceRange subresources =
                    GetSubresourcesAffectedByCopy(copy->destination, copy->copySize);
                const uint8_t* data = buffer->GetMappedData() + copy->source.offset;

                DAWN_TRY(texture->WriteTexture(
                    commandContext, subresources, copy->destination.origin, copy->copySize, data,
                    copy->source.bytesPerRow, copy->source.rowsPerImage));
                break;
            }

            case Command::CopyTextureToBuffer: {
                CopyTextureToBufferCmd* copy = mCommands.NextCommand<CopyTextureToBufferCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }

                auto& src = copy->source;
                auto& dst = copy->destination;

                SubresourceRange subresources = GetSubresourcesAffectedByCopy(src, copy->copySize);
                DAWN_TRY(ToBackend(src.texture)
                             ->EnsureSubresourceContentInitialized(commandContext, subresources));

                // Create a staging texture.
                D3D11_TEXTURE2D_DESC stagingTextureDesc;
                stagingTextureDesc.Width = copy->copySize.width;
                stagingTextureDesc.Height = copy->copySize.height;
                stagingTextureDesc.MipLevels = 1;
                stagingTextureDesc.ArraySize = copy->copySize.depthOrArrayLayers;
                stagingTextureDesc.Format = ToBackend(src.texture)->GetD3D11Format();
                stagingTextureDesc.SampleDesc.Count = 1;
                stagingTextureDesc.SampleDesc.Quality = 0;
                stagingTextureDesc.Usage = D3D11_USAGE_STAGING;
                stagingTextureDesc.BindFlags = 0;
                stagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingTextureDesc.MiscFlags = 0;

                ComPtr<ID3D11Texture2D> stagingTexture;
                DAWN_TRY(CheckHRESULT(commandContext->GetD3D11Device()->CreateTexture2D(
                                          &stagingTextureDesc, nullptr, &stagingTexture),
                                      "D3D11 create staging texture"));

                uint32_t subresource =
                    src.texture->GetSubresourceIndex(src.mipLevel, src.origin.z, src.aspect);

                // Copy the texture to the staging texture.
                D3D11_BOX srcBox;
                srcBox.left = src.origin.x;
                srcBox.right = src.origin.x + copy->copySize.width;
                srcBox.top = src.origin.y;
                srcBox.bottom = src.origin.y + copy->copySize.height;
                srcBox.front = 0;
                srcBox.back = copy->copySize.depthOrArrayLayers;

                d3d11DeviceContext1->CopySubresourceRegion(
                    stagingTexture.Get(), 0, 0, 0, 0, ToBackend(src.texture)->GetD3D11Resource(),
                    subresource, &srcBox);

                // Copy the staging texture to the buffer.
                // The Map() will block until the GPU is done with the texture.
                // TODO(dawn:1705): avoid blocking the CPU.
                D3D11_MAPPED_SUBRESOURCE mappedResource;
                DAWN_TRY(CheckHRESULT(d3d11DeviceContext1->Map(stagingTexture.Get(), 0,
                                                               D3D11_MAP_READ, 0, &mappedResource),
                                      "D3D11 map staging texture"));

                Buffer* buffer = ToBackend(dst.buffer.Get());
                Buffer::ScopedMap scopedMap(buffer, "CopyBufferToTexture");
                DAWN_TRY(scopedMap.AcquireResult());

                DAWN_TRY(buffer->EnsureDataInitializedAsDestination(
                    commandContext, dst.offset, dst.bytesPerRow * copy->copySize.height));

                uint8_t* pDstData = buffer->GetMappedData() + dst.offset;
                uint8_t* pSrcData = reinterpret_cast<uint8_t*>(mappedResource.pData);

                // TODO(dawn:1705): figure out the memcpy size.
                uint32_t memcpySize = std::min(dst.bytesPerRow, mappedResource.RowPitch);
                for (uint32_t y = 0; y < copy->copySize.height; ++y) {
                    memcpy(pDstData, pSrcData, memcpySize);
                    pDstData += dst.bytesPerRow;
                    pSrcData += mappedResource.RowPitch;
                }

                d3d11DeviceContext1->Unmap(stagingTexture.Get(), 0);

                break;
            }

            case Command::CopyTextureToTexture: {
                CopyTextureToTextureCmd* copy = mCommands.NextCommand<CopyTextureToTextureCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }

                auto& src = copy->source;
                auto& dst = copy->destination;

                SubresourceRange subresources = GetSubresourcesAffectedByCopy(src, copy->copySize);
                DAWN_TRY(ToBackend(src.texture)
                             ->EnsureSubresourceContentInitialized(commandContext, subresources));

                subresources = GetSubresourcesAffectedByCopy(dst, copy->copySize);
                if (IsCompleteSubresourceCopiedTo(dst.texture.Get(), copy->copySize,
                                                  dst.mipLevel)) {
                    dst.texture->SetIsSubresourceContentInitialized(true, subresources);
                } else {
                    DAWN_TRY(
                        ToBackend(dst.texture)
                            ->EnsureSubresourceContentInitialized(commandContext, subresources));
                }

                D3D11_BOX srcBox;
                srcBox.left = src.origin.x;
                srcBox.right = src.origin.x + copy->copySize.width;
                srcBox.top = src.origin.y;
                srcBox.bottom = src.origin.y + copy->copySize.height;
                srcBox.front = 0;
                srcBox.back = 1;

                uint32_t subresource =
                    src.texture->GetSubresourceIndex(src.mipLevel, src.origin.z, src.aspect);

                commandContext->GetD3D11DeviceContext()->CopySubresourceRegion(
                    ToBackend(dst.texture)->GetD3D11Resource(), dst.mipLevel, dst.origin.x,
                    dst.origin.y, dst.origin.z, ToBackend(src.texture)->GetD3D11Resource(),
                    subresource, &srcBox);

                break;
            }

            case Command::ClearBuffer: {
                ClearBufferCmd* cmd = mCommands.NextCommand<ClearBufferCmd>();
                if (cmd->size == 0) {
                    // Skip no-op fills.
                    break;
                }
                Buffer* buffer = ToBackend(cmd->buffer.Get());
                DAWN_TRY(buffer->Clear(commandContext, 0, cmd->offset, cmd->size));
                break;
            }

            case Command::ResolveQuerySet: {
                return DAWN_UNIMPLEMENTED_ERROR("ResolveQuerySet unimplemented");
            }

            case Command::WriteTimestamp: {
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");
            }

            case Command::WriteBuffer: {
                WriteBufferCmd* cmd = mCommands.NextCommand<WriteBufferCmd>();
                if (cmd->size == 0) {
                    // Skip no-op writes.
                    continue;
                }

                Buffer* dstBuffer = ToBackend(cmd->buffer.Get());
                uint8_t* data = mCommands.NextData<uint8_t>(cmd->size);
                DAWN_TRY(dstBuffer->Write(commandContext, cmd->offset, data, cmd->size));

                break;
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                return DAWN_UNIMPLEMENTED_ERROR("Debug markers unimplemented");
            }

            default:
                return DAWN_FORMAT_INTERNAL_ERROR("Unknown command type: %d", type);
        }
    }

    return {};
}

MaybeError CommandBuffer::ExecuteComputePass(CommandRecordingContext* commandContext) {
    return DAWN_UNIMPLEMENTED_ERROR("Compute pass unimplemented");
}

MaybeError CommandBuffer::ExecuteRenderPass(BeginRenderPassCmd* renderPass,
                                            CommandRecordingContext* commandContext) {
    return DAWN_UNIMPLEMENTED_ERROR("Render pass unimplemented");
}

}  // namespace dawn::native::d3d11
