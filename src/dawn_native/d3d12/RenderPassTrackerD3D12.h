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

#ifndef DAWNNATIVE_D3D12_RENDERPASSTRACKERD3D12_H_
#define DAWNNATIVE_D3D12_RENDERPASSTRACKERD3D12_H_

#include "common/Constants.h"

#include "dawn_native/d3d12/CommandBufferD3D12.h"
#include "dawn_native/d3d12/TextureD3D12.h"

#include "dawn_native/d3d12/d3d12_platform.h"

#include <array>

namespace dawn_native { namespace d3d12 {

    struct OMSetRenderTargetArgs;
    class RenderPassTracker {
      public:
        RenderPassTracker(const OMSetRenderTargetArgs& args, bool hasUAV);
        const D3D12_RENDER_PASS_RENDER_TARGET_DESC* GetRenderPassRenderTargetDescriptors() const;
        const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* GetRenderPassDepthStencilDescriptor() const;
        D3D12_RENDER_PASS_FLAGS GetRenderPassFlags() const;
        void SetRenderTargetBeginningAccess(uint32_t attachment,
                                            wgpu::LoadOp loadOp,
                                            dawn_native::Color clearColor,
                                            DXGI_FORMAT format);
        void SetRenderTargetEndingAccess(uint32_t attachment, wgpu::StoreOp storeOp);
        void SetRenderTargetEndingAccessResolve(uint32_t attachment,
                                                wgpu::StoreOp storeOp,
                                                TextureView* resolveSource,
                                                TextureView* resolveDestination);

        void SetDepthAccess(wgpu::LoadOp loadOp,
                            wgpu::StoreOp storeOp,
                            float clearDepth,
                            DXGI_FORMAT format);
        void SetStencilAccess(wgpu::LoadOp loadOp,
                              wgpu::StoreOp storeOp,
                              uint8_t clearStencil,
                              DXGI_FORMAT format);
        void SetStencilNoAccess();

      private:
        D3D12_RENDER_PASS_FLAGS mRenderPassFlags = D3D12_RENDER_PASS_FLAG_NONE;
        D3D12_RENDER_PASS_DEPTH_STENCIL_DESC mRenderPassDepthStencilDesc;
        std::array<D3D12_RENDER_PASS_RENDER_TARGET_DESC, kMaxColorAttachments>
            mRenderPassRenderTargetDescriptors;
        std::array<D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS,
                   kMaxColorAttachments>
            mSubresourceParams;
    };
}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RENDERPASSTRACKERD3D12_H_