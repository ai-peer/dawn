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

#include "dawn/native/d3d11/CommandBufferD3D11.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "dawn/native/BindGroup.h"
#include "dawn/native/BindGroupTracker.h"
#include "dawn/native/CommandEncoder.h"
#include "dawn/native/Commands.h"
#include "dawn/native/ExternalTexture.h"
#include "dawn/native/RenderBundle.h"
#include "dawn/native/VertexFormat.h"
#include "dawn/native/d3d11/BufferD3D11.h"
#include "dawn/native/d3d11/ComputePipelineD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/Forward.h"
// #include "dawn/native/d3d11/PersistentPipelineStateD3D11.h"
#include "dawn/native/d3d11/D3D11Error.h"
#include "dawn/native/d3d11/PipelineLayoutD3D11.h"
#include "dawn/native/d3d11/RenderPipelineD3D11.h"
#include "dawn/native/d3d11/SamplerD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {
namespace {

DXGI_FORMAT DXGIIndexFormat(wgpu::IndexFormat format) {
    switch (format) {
        case wgpu::IndexFormat::Uint16:
            return DXGI_FORMAT_R16_UINT;
        case wgpu::IndexFormat::Uint32:
            return DXGI_FORMAT_R32_UINT;
        default:
            UNREACHABLE();
    }
}

class BindGroupTracker : public BindGroupTrackerBase<false, uint64_t> {
  public:
    MaybeError Apply(CommandRecordingContext* commandRecordingContext) {
        BeforeApply();
        for (BindGroupIndex index : IterateBitSet(mDirtyBindGroupsObjectChangedOrIsDynamic)) {
            DAWN_TRY(ApplyBindGroup(commandRecordingContext, index, mBindGroups[index],
                                    mDynamicOffsets[index]));
        }
        AfterApply();
        return {};
    }

  private:
    MaybeError ApplyBindGroup(CommandRecordingContext* commandRecordingContext,
                              BindGroupIndex index,
                              BindGroupBase* group,
                              const ityp::vector<BindingIndex, uint64_t>& dynamicOffsets) {
        const auto& indices = ToBackend(mPipelineLayout)->GetBindingIndexInfo()[index];

        for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
             ++bindingIndex) {
            const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);

            if (bindingInfo.bindingType == BindingInfoType::Texture) {
                // TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                // view->CopyIfNeeded();
            }
        }

        for (BindingIndex bindingIndex{0}; bindingIndex < group->GetLayout()->GetBindingCount();
             ++bindingIndex) {
            const BindingInfo& bindingInfo = group->GetLayout()->GetBindingInfo(bindingIndex);

            switch (bindingInfo.bindingType) {
                case BindingInfoType::Buffer: {
                    BufferBinding binding = group->GetBindingAsBufferBinding(bindingIndex);
                    ID3D11Buffer* d3d11Buffer = ToBackend(binding.buffer)->GetD3D11Buffer();
                    UINT offset = static_cast<UINT>(binding.offset);
                    if (bindingInfo.buffer.hasDynamicOffset) {
                        // Dynamic buffers are packed at the front of BindingIndices.
                        offset += dynamicOffsets[bindingIndex];
                    }

                    auto* deviceContext = commandRecordingContext->GetD3D11DeviceContext1();

                    switch (bindingInfo.buffer.type) {
                        case wgpu::BufferBindingType::Uniform:
                            if (bindingInfo.visibility & wgpu::ShaderStage::Vertex) {
                                ID3D11Buffer* buffers[1] = {d3d11Buffer};
                                deviceContext->VSSetConstantBuffers1(indices[bindingIndex], 1,
                                                                     buffers, &offset, nullptr);
                            }
                            if (bindingInfo.visibility & wgpu::ShaderStage::Fragment) {
                                ID3D11Buffer* buffers[1] = {d3d11Buffer};
                                deviceContext->PSSetConstantBuffers1(indices[bindingIndex], 1,
                                                                     buffers, &offset, nullptr);
                            }
                            break;
                        case wgpu::BufferBindingType::Storage:
                        case wgpu::BufferBindingType::ReadOnlyStorage:
                            return DAWN_UNIMPLEMENTED_ERROR("Storage buffers are not supported");
                        case wgpu::BufferBindingType::Undefined:
                            UNREACHABLE();
                    }
                    break;
                }

                case BindingInfoType::Sampler: {
                    Sampler* sampler = ToBackend(group->GetBindingAsSampler(bindingIndex));
                    ID3D11Device* device = commandRecordingContext->GetD3D11Device();
                    ComPtr<ID3D11SamplerState> samplerState;
                    device->CreateSamplerState(&sampler->GetSamplerDescriptor(), &samplerState);
                    ID3D11SamplerState* samplerStatePtr = samplerState.Get();
                    commandRecordingContext->GetD3D11DeviceContext1()->PSSetSamplers(
                        indices[bindingIndex], 1, &samplerStatePtr);
                    break;
                }

                case BindingInfoType::Texture: {
                    TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                    const D3D11_SHADER_RESOURCE_VIEW_DESC& srvDesc = view->GetSRVDescriptor();
                    ComPtr<ID3D11ShaderResourceView> d3d11ShaderResourceView;

                    ID3D11Device* device = commandRecordingContext->GetD3D11Device();

                    DAWN_TRY(CheckHRESULT(device->CreateShaderResourceView(
                                              ToBackend(view->GetTexture())->GetD3D11Texture(),
                                              &srvDesc, &d3d11ShaderResourceView),
                                          "CreateShaderResourceView"));
                    ID3D11ShaderResourceView* d3d11ShaderResourceViewPtr =
                        d3d11ShaderResourceView.Get();
                    commandRecordingContext->GetD3D11DeviceContext1()->PSSetShaderResources(
                        indices[bindingIndex], 1, &d3d11ShaderResourceViewPtr);
                    break;
                }

                case BindingInfoType::StorageTexture: {
                    // TextureView* view = ToBackend(group->GetBindingAsTextureView(bindingIndex));
                    // Texture* texture = ToBackend(view->GetTexture());
                    // GLuint handle = texture->GetHandle();
                    // GLuint imageIndex = indices[bindingIndex];

                    // GLenum access;
                    // switch (bindingInfo.storageTexture.access) {
                    //     case wgpu::StorageTextureAccess::WriteOnly:
                    //         access = GL_WRITE_ONLY;
                    //         break;
                    //     case wgpu::StorageTextureAccess::Undefined:
                    //         UNREACHABLE();
                    // }

                    // // OpenGL ES only supports either binding a layer or the entire
                    // // texture in glBindImageTexture().
                    // GLboolean isLayered;
                    // if (view->GetLayerCount() == 1) {
                    //     isLayered = GL_FALSE;
                    // } else if (texture->GetArrayLayers() == view->GetLayerCount()) {
                    //     isLayered = GL_TRUE;
                    // } else {
                    //     UNREACHABLE();
                    // }

                    // gl.BindImageTexture(imageIndex, handle, view->GetBaseMipLevel(), isLayered,
                    //                     view->GetBaseArrayLayer(), access,
                    //                     texture->GetGLFormat().internalFormat);
                    // texture->Touch();
                    return DAWN_UNIMPLEMENTED_ERROR("Storage textures are not supported");
                    break;
                }

                case BindingInfoType::ExternalTexture: {
                    return DAWN_UNIMPLEMENTED_ERROR("External textures are not supported");
                    break;
                }
            }
        }
        return {};
    }
};

}  // namespace

// Create CommandBuffer
Ref<CommandBuffer> CommandBuffer::Create(CommandEncoder* encoder,
                                         const CommandBufferDescriptor* descriptor) {
    Ref<CommandBuffer> commandBuffer = AcquireRef(new CommandBuffer(encoder, descriptor));
    return commandBuffer;
}

CommandBuffer::CommandBuffer(CommandEncoder* encoder, const CommandBufferDescriptor* descriptor)
    : CommandBufferBase(encoder, descriptor) {}

MaybeError CommandBuffer::Execute() {
    CommandRecordingContext* commandRecordingContext = nullptr;
    DAWN_TRY_ASSIGN(commandRecordingContext, ToBackend(GetDevice())->GetPendingCommandContext());

    ID3D11DeviceContext1* d3d11DeviceContext1 = commandRecordingContext->GetD3D11DeviceContext1();

    auto LazyClearSyncScope = [](const SyncScopeResourceUsage& scope) {
        // for (size_t i = 0; i < scope.textures.size(); i++) {
        //     Texture* texture = ToBackend(scope.textures[i]);

        //     // Clear subresources that are not render attachments. Render attachments will be
        //     // cleared in RecordBeginRenderPass by setting the loadop to clear when the texture
        //     // subresource has not been initialized before the render pass.
        //     scope.textureUsages[i].Iterate(
        //         [&](const SubresourceRange& range, wgpu::TextureUsage usage) {
        //             if (usage & ~wgpu::TextureUsage::RenderAttachment) {
        //                 // texture->EnsureSubresourceContentInitialized(range);
        //             }
        //         });
        // }

        // for (BufferBase* bufferBase : scope.buffers) {
        //     ToBackend(bufferBase)->EnsureDataInitialized();
        // }
    };

    size_t nextComputePassNumber = 0;
    size_t nextRenderPassNumber = 0;

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::BeginComputePass: {
                mCommands.NextCommand<BeginComputePassCmd>();
                for (const SyncScopeResourceUsage& scope :
                     GetResourceUsages().computePasses[nextComputePassNumber].dispatchUsages) {
                    LazyClearSyncScope(scope);
                }
                DAWN_TRY(ExecuteComputePass(commandRecordingContext));

                nextComputePassNumber++;
                break;
            }

            case Command::BeginRenderPass: {
                auto* cmd = mCommands.NextCommand<BeginRenderPassCmd>();
                LazyClearSyncScope(GetResourceUsages().renderPasses[nextRenderPassNumber]);
                LazyClearRenderPassAttachments(cmd);
                DAWN_TRY(ExecuteRenderPass(cmd, commandRecordingContext));

                nextRenderPassNumber++;
                break;
            }

            case Command::CopyBufferToBuffer: {
                CopyBufferToBufferCmd* copy = mCommands.NextCommand<CopyBufferToBufferCmd>();
                if (copy->size == 0) {
                    // Skip no-op copies.
                    break;
                }

                // TODO: Implement data initialization for buffers.
                DAWN_TRY(ToBackend(copy->source)->EnsureDataInitialized(commandRecordingContext));
                // ToBackend(copy->destination)
                //     ->EnsureDataInitializedAsDestination(copy->destinationOffset, copy->size);

                D3D11_BOX srcBox;
                srcBox.left = copy->sourceOffset;
                srcBox.right = copy->sourceOffset + copy->size;
                srcBox.top = 0;
                srcBox.bottom = 1;
                srcBox.front = 0;
                srcBox.back = 1;
                commandRecordingContext->GetD3D11DeviceContext()->CopySubresourceRegion(
                    ToBackend(copy->destination)->GetD3D11Buffer(), 0, copy->destinationOffset, 0,
                    0, ToBackend(copy->source)->GetD3D11Buffer(), 0, &srcBox);

                // TODO: Implement tracking of buffer usage.
                // ToBackend(copy->source)->TrackUsage();
                // ToBackend(copy->destination)->TrackUsage();

                break;
            }

            case Command::CopyBufferToTexture: {
                CopyBufferToTextureCmd* copy = mCommands.NextCommand<CopyBufferToTextureCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }

                auto& src = copy->source;
                auto& dst = copy->destination;
                Buffer* buffer = ToBackend(src.buffer.Get());

                D3D11_BOX dstBox;
                dstBox.left = dst.origin.x;
                dstBox.right = dst.origin.x + copy->copySize.width;
                dstBox.top = dst.origin.y;
                dstBox.bottom = dst.origin.y + copy->copySize.height;
                dstBox.front = dst.origin.z;
                dstBox.back = dst.origin.z + copy->copySize.depthOrArrayLayers;

                const void* pSrcData = buffer->GetStagingBufferPointer() + src.offset;

                d3d11DeviceContext1->UpdateSubresource(
                    ToBackend(dst.texture.Get())->GetD3D11Texture(), dst.mipLevel, &dstBox,
                    pSrcData, src.bytesPerRow, src.rowsPerImage * src.bytesPerRow);

                // DAWN_INVALID_IF(
                //     dst.aspect == Aspect::Stencil,
                //     "Copies to stencil textures are unsupported on the OpenGL backend.");
                // ASSERT(dst.aspect == Aspect::Color);
                // DAWN_TRY(buffer->EnsureDataInitialized(commandRecordingContext));
                // SubresourceRange range = GetSubresourcesAffectedByCopy(dst, copy->copySize);
                // if (IsCompleteSubresourceCopiedTo(dst.texture.Get(), copy->copySize,
                //                                   dst.mipLevel)) {
                //     dst.texture->SetIsSubresourceContentInitialized(true, range);
                // } else {
                //     ToBackend(dst.texture)->EnsureSubresourceContentInitialized(range);
                // }

                // gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer->GetHandle());

                // TextureDataLayout dataLayout;
                // dataLayout.offset = 0;
                // dataLayout.bytesPerRow = src.bytesPerRow;
                // dataLayout.rowsPerImage = src.rowsPerImage;

                // DoTexSubImage(gl, dst, reinterpret_cast<void*>(src.offset), dataLayout,
                //               copy->copySize);
                // gl.BindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                // ToBackend(dst.texture)->Touch();

                // buffer->TrackUsage();
                // return DAWN_UNIMPLEMENTED_ERROR("CopyBufferToTexture");
                break;
            }

            case Command::CopyTextureToBuffer: {
                CopyTextureToBufferCmd* copy = mCommands.NextCommand<CopyTextureToBufferCmd>();
                if (copy->copySize.width == 0 || copy->copySize.height == 0 ||
                    copy->copySize.depthOrArrayLayers == 0) {
                    // Skip no-op copies.
                    continue;
                }
                return DAWN_UNIMPLEMENTED_ERROR("CopyTextureToBuffer");
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

                // TODO: Implement data initialization for textures.
                // ToBackend(src.texture)->EnsureSubresourceContentInitialized(srcRange);

                D3D11_BOX srcBox;
                srcBox.left = src.origin.x;
                srcBox.right = src.origin.x + copy->copySize.width;
                srcBox.top = src.origin.y;
                srcBox.bottom = src.origin.y + copy->copySize.height;
                srcBox.front = src.origin.z;
                srcBox.back = src.origin.z + copy->copySize.depthOrArrayLayers;

                commandRecordingContext->GetD3D11DeviceContext()->CopySubresourceRegion(
                    ToBackend(dst.texture)->GetD3D11Texture(), dst.mipLevel, dst.origin.x,
                    dst.origin.y, dst.origin.z, ToBackend(src.texture)->GetD3D11Texture(),
                    src.mipLevel, &srcBox);

                // TODO: Implement tracking of texture usage.
                // ToBackend(dst.texture)->Touch();
                break;
            }

            case Command::ClearBuffer: {
                ClearBufferCmd* cmd = mCommands.NextCommand<ClearBufferCmd>();
                if (cmd->size == 0) {
                    // Skip no-op fills.
                    break;
                }
                // Buffer* dstBuffer = ToBackend(cmd->buffer.Get());

                // bool clearedToZero =
                //     dstBuffer->EnsureDataInitializedAsDestination(cmd->offset, cmd->size);

                // if (!clearedToZero) {
                //     const std::vector<uint8_t> clearValues(cmd->size, 0u);
                //     gl.BindBuffer(GL_ARRAY_BUFFER, dstBuffer->GetHandle());
                //     gl.BufferSubData(GL_ARRAY_BUFFER, cmd->offset, cmd->size,
                //     clearValues.data());
                // }

                // dstBuffer->TrackUsage();
                return DAWN_UNIMPLEMENTED_ERROR("ClearBuffer");
                break;
            }

            case Command::ResolveQuerySet: {
                // TODO(crbug.com/dawn/434): Resolve non-precise occlusion query.
                SkipCommand(&mCommands, type);
                return DAWN_UNIMPLEMENTED_ERROR("ResolveQuerySet unimplemented");
                break;
            }

            case Command::WriteTimestamp: {
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                // Due to lack of linux driver support for GL_EXT_debug_marker
                // extension these functions are skipped.
                SkipCommand(&mCommands, type);
                break;
            }

            case Command::WriteBuffer: {
                [[maybe_unused]] WriteBufferCmd* write = mCommands.NextCommand<WriteBufferCmd>();
                // uint64_t offset = write->offset;
                // uint64_t size = write->size;
                // if (size == 0) {
                //     continue;
                // }

                // Buffer* dstBuffer = ToBackend(write->buffer.Get());
                // uint8_t* data = mCommands.NextData<uint8_t>(size);
                // dstBuffer->EnsureDataInitializedAsDestination(offset, size);

                // gl.BindBuffer(GL_ARRAY_BUFFER, dstBuffer->GetHandle());
                // gl.BufferSubData(GL_ARRAY_BUFFER, offset, size, data);

                // dstBuffer->TrackUsage();
                return DAWN_UNIMPLEMENTED_ERROR("WriteBuffer unimplemented");
                break;
            }

            default:
                return DAWN_FORMAT_INTERNAL_ERROR("Unknown command type: %d", type);
        }
    }

    return {};
}

MaybeError CommandBuffer::ExecuteComputePass(CommandRecordingContext* commandRecordingContext) {
    ComputePipeline* lastPipeline = nullptr;
    BindGroupTracker bindGroupTracker = {};

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::EndComputePass: {
                mCommands.NextCommand<EndComputePassCmd>();
                return DAWN_UNIMPLEMENTED_ERROR("EndComputePass unimplemented");
                return {};
            }

            case Command::Dispatch: {
                [[maybe_unused]] DispatchCmd* dispatch = mCommands.NextCommand<DispatchCmd>();

                DAWN_TRY(bindGroupTracker.Apply(commandRecordingContext));

                // gl.DispatchCompute(dispatch->x, dispatch->y, dispatch->z);
                // gl.MemoryBarrier(GL_ALL_BARRIER_BITS);
                return DAWN_UNIMPLEMENTED_ERROR("Dispatch unimplemented");
                break;
            }

            case Command::DispatchIndirect: {
                [[maybe_unused]] DispatchIndirectCmd* dispatch =
                    mCommands.NextCommand<DispatchIndirectCmd>();

                DAWN_TRY(bindGroupTracker.Apply(commandRecordingContext));

                // uint64_t indirectBufferOffset = dispatch->indirectOffset;
                // Buffer* indirectBuffer = ToBackend(dispatch->indirectBuffer.Get());

                // gl.BindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBuffer->GetHandle());
                // gl.DispatchComputeIndirect(static_cast<GLintptr>(indirectBufferOffset));
                // gl.MemoryBarrier(GL_ALL_BARRIER_BITS);

                // indirectBuffer->TrackUsage();
                return DAWN_UNIMPLEMENTED_ERROR("DispatchIndirect unimplemented");
                break;
            }

            case Command::SetComputePipeline: {
                SetComputePipelineCmd* cmd = mCommands.NextCommand<SetComputePipelineCmd>();
                lastPipeline = ToBackend(cmd->pipeline).Get();
                lastPipeline->ApplyNow();
                bindGroupTracker.OnSetPipeline(lastPipeline);
                break;
            }

            case Command::SetBindGroup: {
                SetBindGroupCmd* cmd = mCommands.NextCommand<SetBindGroupCmd>();

                uint32_t* dynamicOffsets = nullptr;
                if (cmd->dynamicOffsetCount > 0) {
                    dynamicOffsets = mCommands.NextData<uint32_t>(cmd->dynamicOffsetCount);
                }

                bindGroupTracker.OnSetBindGroup(cmd->index, cmd->group.Get(),
                                                cmd->dynamicOffsetCount, dynamicOffsets);

                break;
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                // Due to lack of linux driver support for GL_EXT_debug_marker
                // extension these functions are skipped.
                SkipCommand(&mCommands, type);
                break;
            }

            case Command::WriteTimestamp: {
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");
            }

            default:
                UNREACHABLE();
        }
    }

    // EndComputePass should have been called
    UNREACHABLE();
}

MaybeError CommandBuffer::ExecuteRenderPass(BeginRenderPassCmd* renderPass,
                                            CommandRecordingContext* commandRecordingContext) {
    ID3D11Device* d3d11Device = commandRecordingContext->GetD3D11Device();
    ID3D11DeviceContext1* d3d11DeviceContext1 = commandRecordingContext->GetD3D11DeviceContext1();

    ityp::array<ColorAttachmentIndex, ComPtr<ID3D11RenderTargetView>, kMaxColorAttachments>
        d3d11RenderTargetViews = {};
    ityp::array<ColorAttachmentIndex, ID3D11RenderTargetView*, kMaxColorAttachments>
        d3d11RenderTargetViewPtrs = {};
    ColorAttachmentIndex attachmentCount(uint8_t(0));
    for (ColorAttachmentIndex i :
         IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
        TextureView* colorTextureView = ToBackend(renderPass->colorAttachments[i].view.Get());
        Texture* colorTexture = ToBackend(colorTextureView->GetTexture());
        const D3D11_RENDER_TARGET_VIEW_DESC& rtvDesc = colorTextureView->GetRTVDescriptor();
        DAWN_TRY(
            CheckHRESULT(d3d11Device->CreateRenderTargetView(colorTexture->GetD3D11Texture(),
                                                             &rtvDesc, &d3d11RenderTargetViews[i]),
                         "create render target view"));
        if (renderPass->colorAttachments[i].loadOp == wgpu::LoadOp::Clear) {
            d3d11DeviceContext1->ClearRenderTargetView(
                d3d11RenderTargetViews[i].Get(),
                ConvertToFloatColor(renderPass->colorAttachments[i].clearColor).data());
        }
        d3d11RenderTargetViewPtrs[i] = d3d11RenderTargetViews[i].Get();
        attachmentCount = i;
        attachmentCount++;
    }

    ComPtr<ID3D11DepthStencilView> d3d11DepthStencilView;
    if (renderPass->attachmentState->HasDepthStencilAttachment()) {
        auto* attachmentInfo = &renderPass->depthStencilAttachment;
        const Format& attachmentFormat = attachmentInfo->view->GetTexture()->GetFormat();

        TextureView* depthStencilTextureView =
            ToBackend(renderPass->depthStencilAttachment.view.Get());
        Texture* depthStencilTexture = ToBackend(depthStencilTextureView->GetTexture());
        const D3D11_DEPTH_STENCIL_VIEW_DESC& dsvDesc =
            depthStencilTextureView->GetDSVDescriptor(false, false);
        DAWN_TRY(
            CheckHRESULT(d3d11Device->CreateDepthStencilView(depthStencilTexture->GetD3D11Texture(),
                                                             &dsvDesc, &d3d11DepthStencilView),
                         "create depth stencil view"));

        UINT clearFlags = 0;
        if (attachmentFormat.HasDepth() &&
            renderPass->depthStencilAttachment.depthLoadOp == wgpu::LoadOp::Clear) {
            clearFlags |= D3D11_CLEAR_DEPTH;
        }

        if (attachmentFormat.HasStencil() &&
            renderPass->depthStencilAttachment.stencilLoadOp == wgpu::LoadOp::Clear) {
            clearFlags |= D3D11_CLEAR_STENCIL;
        }

        d3d11DeviceContext1->ClearDepthStencilView(d3d11DepthStencilView.Get(), clearFlags,
                                                   attachmentInfo->clearDepth,
                                                   attachmentInfo->clearStencil);
    }

    d3d11DeviceContext1->OMSetRenderTargets(static_cast<uint8_t>(attachmentCount),
                                            d3d11RenderTargetViewPtrs.data(),
                                            d3d11DepthStencilView.Get());

    // Set the default values for the dynamic state:
    // Set blend color
    constexpr float kBlendColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    constexpr UINT kSampleMask = 0xFFFFFFFF;
    d3d11DeviceContext1->OMSetBlendState(/*pBlendState=*/nullptr, kBlendColor, kSampleMask);

    // Set viewport
    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = renderPass->width;
    viewport.Height = renderPass->height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    d3d11DeviceContext1->RSSetViewports(1, &viewport);

    // Set scissor
    D3D11_RECT scissor;
    scissor.left = 0;
    scissor.top = 0;
    scissor.right = renderPass->width;
    scissor.bottom = renderPass->height;
    d3d11DeviceContext1->RSSetScissorRects(1, &scissor);

    RenderPipeline* lastPipeline = nullptr;
    // VertexStateBufferBindingTracker vertexStateBufferBindingTracker;
    BindGroupTracker bindGroupTracker = {};

    auto DoRenderBundleCommand = [&](CommandIterator* iter, Command type) -> MaybeError {
        switch (type) {
            case Command::Draw: {
                DrawCmd* draw = iter->NextCommand<DrawCmd>();

                // vertexStateBufferBindingTracker.Apply(gl);
                DAWN_TRY(bindGroupTracker.Apply(commandRecordingContext));

                commandRecordingContext->GetD3D11DeviceContext()->DrawInstanced(
                    draw->vertexCount, draw->instanceCount, draw->firstVertex, draw->firstInstance);
                break;
            }

            case Command::DrawIndexed: {
                DrawIndexedCmd* draw = iter->NextCommand<DrawIndexedCmd>();

                // vertexStateBufferBindingTracker.Apply(gl);
                DAWN_TRY(bindGroupTracker.Apply(commandRecordingContext));

                commandRecordingContext->GetD3D11DeviceContext()->DrawIndexedInstanced(
                    draw->indexCount, draw->instanceCount, draw->firstIndex, draw->baseVertex,
                    draw->firstInstance);

                break;
            }

            case Command::DrawIndirect: {
                DrawIndirectCmd* draw = iter->NextCommand<DrawIndirectCmd>();

                // vertexStateBufferBindingTracker.Apply(gl);
                DAWN_TRY(bindGroupTracker.Apply(commandRecordingContext));

                uint64_t indirectBufferOffset = draw->indirectOffset;
                Buffer* indirectBuffer = ToBackend(draw->indirectBuffer.Get());
                ASSERT(indirectBuffer != nullptr);

                commandRecordingContext->GetD3D11DeviceContext()->DrawInstancedIndirect(
                    indirectBuffer->GetD3D11Buffer(), indirectBufferOffset);
                // TODO: track usage
                // indirectBuffer->TrackUsage();

                break;
            }

            case Command::DrawIndexedIndirect: {
                DrawIndexedIndirectCmd* draw = iter->NextCommand<DrawIndexedIndirectCmd>();

                // vertexStateBufferBindingTracker.Apply(gl);
                DAWN_TRY(bindGroupTracker.Apply(commandRecordingContext));

                Buffer* indirectBuffer = ToBackend(draw->indirectBuffer.Get());
                ASSERT(indirectBuffer != nullptr);

                commandRecordingContext->GetD3D11DeviceContext()->DrawIndexedInstancedIndirect(
                    indirectBuffer->GetD3D11Buffer(), draw->indirectOffset);

                // TODO: track usage
                // indirectBuffer->TrackUsage();
                break;
            }

            case Command::InsertDebugMarker:
            case Command::PopDebugGroup:
            case Command::PushDebugGroup: {
                // Due to lack of linux driver support for GL_EXT_debug_marker
                // extension these functions are skipped.
                SkipCommand(iter, type);
                break;
            }

            case Command::SetRenderPipeline: {
                SetRenderPipelineCmd* cmd = iter->NextCommand<SetRenderPipelineCmd>();

                lastPipeline = ToBackend(cmd->pipeline).Get();
                DAWN_TRY(lastPipeline->ApplyNow(commandRecordingContext));
                // vertexStateBufferBindingTracker.OnSetPipeline(lastPipeline);
                bindGroupTracker.OnSetPipeline(lastPipeline);

                // return DAWN_UNIMPLEMENTED_ERROR("SetRenderPipeline");
                break;
            }

            case Command::SetBindGroup: {
                SetBindGroupCmd* cmd = iter->NextCommand<SetBindGroupCmd>();

                uint32_t* dynamicOffsets = nullptr;
                if (cmd->dynamicOffsetCount > 0) {
                    dynamicOffsets = iter->NextData<uint32_t>(cmd->dynamicOffsetCount);
                }
                bindGroupTracker.OnSetBindGroup(cmd->index, cmd->group.Get(),
                                                cmd->dynamicOffsetCount, dynamicOffsets);

                break;
            }

            case Command::SetIndexBuffer: {
                SetIndexBufferCmd* cmd = iter->NextCommand<SetIndexBufferCmd>();

                UINT indexBufferBaseOffset = cmd->offset;
                DXGI_FORMAT indexBufferFormat = DXGIIndexFormat(cmd->format);

                commandRecordingContext->GetD3D11DeviceContext()->IASetIndexBuffer(
                    ToBackend(cmd->buffer)->GetD3D11Buffer(), indexBufferFormat,
                    indexBufferBaseOffset);

                // TODO: Track usage
                // ToBackend(cmd->buffer)->TrackUsage();

                break;
            }

            case Command::SetVertexBuffer: {
                SetVertexBufferCmd* cmd = iter->NextCommand<SetVertexBufferCmd>();
                ASSERT(lastPipeline);
                const VertexBufferInfo& info = lastPipeline->GetVertexBuffer(cmd->slot);

                // TODO: track all vertex buffers.
                UINT slot = static_cast<uint8_t>(cmd->slot);
                ID3D11Buffer* buffer = ToBackend(cmd->buffer)->GetD3D11Buffer();
                UINT arrayStride = info.arrayStride;
                UINT offset = cmd->offset;
                commandRecordingContext->GetD3D11DeviceContext()->IASetVertexBuffers(
                    slot, 1, &buffer, &arrayStride, &offset);

                // TODO: Track usage
                // ToBackend(cmd->buffer)->TrackUsage();
                break;
            }

            default:
                UNREACHABLE();
                break;
        }

        return {};
    };

    Command type;
    while (mCommands.NextCommandId(&type)) {
        switch (type) {
            case Command::EndRenderPass: {
                mCommands.NextCommand<EndRenderPassCmd>();

                // for (ColorAttachmentIndex i :
                //      IterateBitSet(renderPass->attachmentState->GetColorAttachmentsMask())) {
                //     TextureView* textureView =
                //         ToBackend(renderPass->colorAttachments[i].view.Get());
                //     ToBackend(textureView->GetTexture())->Touch();
                // }
                // if (renderPass->attachmentState->HasDepthStencilAttachment()) {
                //     TextureView* textureView =
                //         ToBackend(renderPass->depthStencilAttachment.view.Get());
                //     ToBackend(textureView->GetTexture())->Touch();
                // }
                // if (renderPass->attachmentState->GetSampleCount() > 1) {
                //     ResolveMultisampledRenderTargets(gl, renderPass);
                // }
                // gl.DeleteFramebuffers(1, &fbo);
                // return DAWN_UNIMPLEMENTED_ERROR("EndRenderPass");
                return {};
            }

            case Command::SetStencilReference: {
                [[maybe_unused]] SetStencilReferenceCmd* cmd =
                    mCommands.NextCommand<SetStencilReferenceCmd>();
                return DAWN_UNIMPLEMENTED_ERROR("SetStencilReference unimplemented");
            }

            case Command::SetViewport: {
                SetViewportCmd* cmd = mCommands.NextCommand<SetViewportCmd>();

                D3D11_VIEWPORT viewport;
                viewport.TopLeftX = cmd->x;
                viewport.TopLeftY = cmd->y;
                viewport.Width = cmd->width;
                viewport.Height = cmd->height;
                viewport.MinDepth = cmd->minDepth;
                viewport.MaxDepth = cmd->maxDepth;
                commandRecordingContext->GetD3D11DeviceContext()->RSSetViewports(1, &viewport);
                break;
            }

            case Command::SetScissorRect: {
                SetScissorRectCmd* cmd = mCommands.NextCommand<SetScissorRectCmd>();

                D3D11_RECT scissorRect = {static_cast<LONG>(cmd->x), static_cast<LONG>(cmd->y),
                                          static_cast<LONG>(cmd->x + cmd->width),
                                          static_cast<LONG>(cmd->y + cmd->height)};
                commandRecordingContext->GetD3D11DeviceContext()->RSSetScissorRects(1,
                                                                                    &scissorRect);

                break;
            }

            case Command::SetBlendConstant: {
                SetBlendConstantCmd* cmd = mCommands.NextCommand<SetBlendConstantCmd>();

                const std::array<float, 4> blendColor = ConvertToFloatColor(cmd->color);
                // TODO: BlendState?
                commandRecordingContext->GetD3D11DeviceContext()->OMSetBlendState(
                    /*pBlendState=*/nullptr, blendColor.data(), /*SampleMask=*/0xFFFFFFFF);

                break;
            }

            case Command::ExecuteBundles: {
                [[maybe_unused]] ExecuteBundlesCmd* cmd =
                    mCommands.NextCommand<ExecuteBundlesCmd>();
                [[maybe_unused]] auto bundles =
                    mCommands.NextData<Ref<RenderBundleBase>>(cmd->count);
                for (uint32_t i = 0; i < cmd->count; ++i) {
                    CommandIterator* iter = bundles[i]->GetCommands();
                    iter->Reset();
                    while (iter->NextCommandId(&type)) {
                        DAWN_TRY(DoRenderBundleCommand(iter, type));
                    }
                }
                break;
            }

            case Command::BeginOcclusionQuery: {
                return DAWN_UNIMPLEMENTED_ERROR("BeginOcclusionQuery unimplemented.");
            }

            case Command::EndOcclusionQuery: {
                return DAWN_UNIMPLEMENTED_ERROR("EndOcclusionQuery unimplemented.");
            }

            case Command::WriteTimestamp:
                return DAWN_UNIMPLEMENTED_ERROR("WriteTimestamp unimplemented");

            default: {
                DAWN_TRY(DoRenderBundleCommand(&mCommands, type));
            }
        }
    }

    // EndRenderPass should have been called
    UNREACHABLE();
}

}  // namespace dawn::native::d3d11
