// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_DAWN_NATIVE_BLITCOLORTOCOLORWITHDRAW_H_
#define SRC_DAWN_NATIVE_BLITCOLORTOCOLORWITHDRAW_H_

#include <bitset>

#include "absl/container/flat_hash_map.h"
#include "dawn/common/ityp_array.h"
#include "dawn/common/ityp_bitset.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"

namespace dawn::native {

class DeviceBase;
class RenderPassEncoder;
struct RenderPassDescriptor;
class TextureViewBase;

struct BlitColorToColorWithDrawPipelineKey {
    BlitColorToColorWithDrawPipelineKey() {
        colorTargetFormats.fill(wgpu::TextureFormat::Undefined);
    }

    PerColorAttachment<wgpu::TextureFormat> colorTargetFormats;
    ColorAttachmentMask attachmentsToExpandResolve;
    ColorAttachmentMask resolveTargetsMask;
    ColorAttachmentMask blitSubsetMask;
    wgpu::TextureFormat depthStencilFormat = wgpu::TextureFormat::Undefined;
    uint32_t sampleCount = 1;

    struct HashFunc {
        size_t operator()(const BlitColorToColorWithDrawPipelineKey& key) const;
    };

    struct EqualityFunc {
        bool operator()(const BlitColorToColorWithDrawPipelineKey& a,
                        const BlitColorToColorWithDrawPipelineKey& b) const;
    };
};

using BlitColorToColorWithDrawPipelinesCache =
    absl::flat_hash_map<BlitColorToColorWithDrawPipelineKey,
                        Ref<RenderPipelineBase>,
                        BlitColorToColorWithDrawPipelineKey::HashFunc,
                        BlitColorToColorWithDrawPipelineKey::EqualityFunc>;

// This function performs the ExpandResolveTexture load operation for the render pass by blitting
// the resolve target to the MSAA attachment.
//
// The function assumes that the render pass is already started. It won't break the render pass,
// just performing a draw call to blit.
// - subsetMask parameter denotes a subset of the color attachments that can be blitted to.
// Note: we don't change the render pass' list of attachments that have ExpandResolveTexture load
// op. Because it's required for the compatibility between the generated pipeline and the render
// pass.
// - useSpecialSampleType: whether we should use kInternalResolveAttachmentSampleType for
// BindGroupLayout or not. This will skip the validation that prevents a texture from being sampled
// and resolved to in the same pass.
MaybeError ExpandResolveTextureWithDraw(DeviceBase* device,
                                        RenderPassEncoder* renderEncoder,
                                        ColorAttachmentMask subsetMask,
                                        bool useSpecialSampleType,
                                        const RenderPassDescriptor* renderPassDescriptor);

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_BLITCOLORTOCOLORWITHDRAW_H_
