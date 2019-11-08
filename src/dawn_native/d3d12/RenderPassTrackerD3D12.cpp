#include "RenderPassTrackerD3D12.h"
// Copyright 2019 The Dawn Authors
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

#include "dawn_native/d3d12/RenderPassTrackerD3D12.h"

#include "dawn_native/Format.h"
#include "dawn_native/d3d12/Forward.h"

#include "dawn_native/dawn_platform.h"

namespace dawn_native { namespace d3d12 {

    namespace {
        D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE D3D12BeginningAccessType(wgpu::LoadOp loadOp) {
            switch (loadOp) {
                case wgpu::LoadOp::Clear:
                    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
                case wgpu::LoadOp::Load:
                    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
                default:
                    UNREACHABLE();
            }
        }

        D3D12_RENDER_PASS_ENDING_ACCESS_TYPE D3D12EndingAccessType(wgpu::StoreOp storeOp) {
            switch (storeOp) {
                case wgpu::StoreOp::Clear:
                    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
                case wgpu::StoreOp::Store:
                    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
                default:
                    UNREACHABLE();
            }
        }
    }  // anonymous namespace

    RenderPassTracker::RenderPassTracker(const OMSetRenderTargetArgs& args, bool hasUAV)
        : mColorAttachmentCount(args.numRTVs), mRenderTargetViews(args.RTVs.data()) {
        for (uint32_t i = 0; i < mColorAttachmentCount; i++) {
            mRenderPassRenderTargetDescriptors[i].cpuDescriptor = args.RTVs[i];
        }

        mRenderPassDepthStencilDesc.cpuDescriptor = args.dsv;

        if (hasUAV) {
            mRenderPassFlags = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES;
        }
    }

    uint32_t RenderPassTracker::GetColorAttachmentCount() const {
        return mColorAttachmentCount;
    }

    bool RenderPassTracker::HasDepth() const {
        return mHasDepth;
    }

    const D3D12_RENDER_PASS_RENDER_TARGET_DESC*
    RenderPassTracker::GetRenderPassRenderTargetDescriptors() const {
        return mRenderPassRenderTargetDescriptors.data();
    }

    const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC*
    RenderPassTracker::GetRenderPassDepthStencilDescriptor() const {
        return &mRenderPassDepthStencilDesc;
    }

    D3D12_RENDER_PASS_FLAGS RenderPassTracker::GetRenderPassFlags() const {
        return mRenderPassFlags;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE* RenderPassTracker::GetRenderTargetViews() const {
        return mRenderTargetViews;
    }

    void RenderPassTracker::SetRenderTargetBeginningAccess(uint32_t attachment,
                                                           wgpu::LoadOp loadOp,
                                                           dawn_native::Color clearColor,
                                                           DXGI_FORMAT format) {
        mRenderPassRenderTargetDescriptors[attachment].BeginningAccess.Type =
            D3D12BeginningAccessType(loadOp);
        if (loadOp == wgpu::LoadOp::Clear) {
            mRenderPassRenderTargetDescriptors[attachment]
                .BeginningAccess.Clear.ClearValue.Color[0] = clearColor.r;
            mRenderPassRenderTargetDescriptors[attachment]
                .BeginningAccess.Clear.ClearValue.Color[1] = clearColor.g;
            mRenderPassRenderTargetDescriptors[attachment]
                .BeginningAccess.Clear.ClearValue.Color[2] = clearColor.b;
            mRenderPassRenderTargetDescriptors[attachment]
                .BeginningAccess.Clear.ClearValue.Color[3] = clearColor.a;
            mRenderPassRenderTargetDescriptors[attachment].BeginningAccess.Clear.ClearValue.Format =
                format;
        }
    }

    void RenderPassTracker::SetRenderTargetEndingAccess(uint32_t attachment,
                                                        wgpu::StoreOp storeOp) {
        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Type =
            D3D12EndingAccessType(storeOp);
    }

    void RenderPassTracker::SetRenderTargetEndingAccessResolve(uint32_t attachment,
                                                               wgpu::StoreOp storeOp,
                                                               TextureView* resolveSource,
                                                               TextureView* resolveDestination) {
        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Type =
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.Format =
            resolveDestination->GetD3D12Format();
        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.pSrcResource =
            ToBackend(resolveSource->GetTexture())->GetD3D12Resource();
        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.pDstResource =
            ToBackend(resolveDestination->GetTexture())->GetD3D12Resource();

        // Clear or preserve the resolve source.
        if (storeOp == wgpu::StoreOp::Clear) {
            mRenderPassRenderTargetDescriptors[attachment]
                .EndingAccess.Resolve.PreserveResolveSource = false;
        } else if (storeOp == wgpu::StoreOp::Store) {
            mRenderPassRenderTargetDescriptors[attachment]
                .EndingAccess.Resolve.PreserveResolveSource = true;
        }

        // RESOLVE_MODE_AVERAGE is only valid for non-integer formats.
        // RESOLVE_MODE_MAX was chosen arbitrarily for integer formats.
        switch (resolveDestination->GetFormat().type) {
            case Format::Type::Sint:
            case Format::Type::Uint:
                mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.ResolveMode =
                    D3D12_RESOLVE_MODE_MAX;
                break;
            default:
                mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.ResolveMode =
                    D3D12_RESOLVE_MODE_AVERAGE;
                break;
        }

        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.SubresourceCount = 1;

        mSubresourceParams[attachment].DstX = 0;
        mSubresourceParams[attachment].DstY = 0;
        mSubresourceParams[attachment].SrcSubresource = 0;
        mSubresourceParams[attachment].DstSubresource =
            ToBackend(resolveDestination->GetTexture())
                ->GetSubresourceIndex(resolveDestination->GetBaseMipLevel(),
                                      resolveDestination->GetBaseArrayLayer());
        mSubresourceParams[attachment].SrcRect = {
            0, 0, ToBackend(resolveDestination->GetTexture())->GetSize().width,
            ToBackend(resolveDestination->GetTexture())->GetSize().height};

        mRenderPassRenderTargetDescriptors[attachment].EndingAccess.Resolve.pSubresourceParameters =
            &mSubresourceParams[attachment];
    }

    void RenderPassTracker::SetDepthAccess(wgpu::LoadOp loadOp,
                                           wgpu::StoreOp storeOp,
                                           float clearDepth,
                                           DXGI_FORMAT format) {
        mHasDepth = true;
        mRenderPassDepthStencilDesc.DepthBeginningAccess.Type = D3D12BeginningAccessType(loadOp);
        if (loadOp == wgpu::LoadOp::Clear) {
            mRenderPassDepthStencilDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth =
                clearDepth;
            mRenderPassDepthStencilDesc.DepthBeginningAccess.Clear.ClearValue.Format = format;
        }
        mRenderPassDepthStencilDesc.DepthEndingAccess.Type = D3D12EndingAccessType(storeOp);
    }

    void RenderPassTracker::SetStencilAccess(wgpu::LoadOp loadOp,
                                             wgpu::StoreOp storeOp,
                                             uint8_t clearStencil,
                                             DXGI_FORMAT format) {
        mRenderPassDepthStencilDesc.StencilBeginningAccess.Type = D3D12BeginningAccessType(loadOp);
        if (loadOp == wgpu::LoadOp::Clear) {
            mRenderPassDepthStencilDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil
                .Stencil = clearStencil;
            mRenderPassDepthStencilDesc.StencilBeginningAccess.Clear.ClearValue.Format = format;
        }
        mRenderPassDepthStencilDesc.StencilEndingAccess.Type = D3D12EndingAccessType(storeOp);
    }

    void RenderPassTracker::SetStencilNoAccess() {
        mRenderPassDepthStencilDesc.StencilBeginningAccess.Type =
            D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        mRenderPassDepthStencilDesc.StencilEndingAccess.Type =
            D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
    }

}}  // namespace dawn_native::d3d12