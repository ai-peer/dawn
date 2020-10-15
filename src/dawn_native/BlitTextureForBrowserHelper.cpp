// Copyright 2020 The Dawn Authors
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

#include "dawn_native/BlitTextureForBrowserHelper.h"

#include "dawn_native/Device.h"
#include "dawn_native/Texture.h"

namespace dawn_native {
    namespace {
        // TODO(shaobo.yan@intel.com): Expand supported blit texture formats
        static constexpr std::unordered_set<wgpu::TextureFormat> validBlitSrcTextureFormats = {
            wgpu::TextureFormat::RGBA8Unorm};
        static constexpr std::unordered_set<wgpu::TextureFormat> validBlitDstTextureFormats = {
            wgpu::TextureFormat::RGBA8Unorm};

        static const float rectVertices[] = {
            // position      texCoord
            1.0, 1.0, 0.0, 1.0, 0.0, 1.0,  -1.0, 0.0, 1.1, 1.1, -1.0, -1.0, 0.0, 0.0, 1.0,
            1.0, 1.0, 0.0, 1.0, 0.0, -1.0, -1.0, 0.0, 0.0, 1.0, -1.0, 1.0,  0.0, 0.0, 0.0,
        };

        MaybeError ValidateFormatConversion(const wgpu::TextureFormat srcFormat,
                                            const wgpu::TextureFormat dstFormat) {
            if (validBlitSrcTextureFormats.find(srcFormat) == validBlitSrcTextureFormats.end() ||
                validBlitDstTextureFormats.find(dstFormat) == validBlitDstTextureFormats.end()) {
                return DAWN_VALIDATION_ERROR(
                    "Unsupported texture formats for BlitTextureForBrowser.");
            }

            return {};
        }
    }  // anonymous namespace

    BlitTextureForBrowserHelper::BlitTextureForBrowserHelper(DeviceBase* device)
        : ObjectBase(device) {
        BufferDescriptor vertexBufferDesc = {};
        vertexBufferDesc.usage = wgpu::BufferUsage::Vertex;
        vertexBufferDesc.size = sizeof(rectVertices);
        mVertexBuffer = AcquireRef(GetDevice()->CreateBuffer(&vertexBufferDesc));
        GetDevice()->GetDefaultQueue()->WriteBuffer(mVertexBuffer.Get(), 0, rectVertices,
                                                    sizeof(rectVertices));

        BufferDescriptor rotationUniformDesc = {};
        rotationUniformDesc.usage = wgpu::BufferUsage::Uniform;
        rotationUniformDesc.size = 64;  // 4x4 matrix for rotation
        mRotationUniform = AcquireRef(GetDevice()->CreateBuffer(&rotationUniformDesc));
    }

    MaybeError BlitTextureForBrowserHelper::ValidateBlitForBrowser(
        const TextureCopyView* source,
        const TextureCopyView* destination,
        const Extent3D* copySize) {
        DAWN_TRY(GetDevice()->ValidateObject(source->texture));
        DAWN_TRY(GetDevice()->ValidateObject(destination->texture));

        DAWN_TRY(ValidateTextureCopyView(GetDevice(), *source, *copySize));
        DAWN_TRY(ValidateTextureCopyView(GetDevice(), *destination, *copySize));

        DAWN_TRY(ValidateTextureToTextureCopyRestrictions(*source, *destination, *copySize));

        DAWN_TRY(ValidateTextureCopyRange(*source, *copySize));
        DAWN_TRY(ValidateTextureCopyRange(*destination, *copySize));

        DAWN_TRY(ValidateCanUseAs(source->texture, wgpu::TextureUsage::CopySrc));
        DAWN_TRY(ValidateCanUseAs(destination->texture, wgpu::TextureUsage::CopyDst));

        DAWN_TRY(ValidateFormatConversion(source->texture->GetFormat().format,
                                          destination->texture->GetFormat().format));

        // TODO(shaobo.yan@intel.com): Support the simplest case for now that source and destination
        // texture has the same size and do full texture blit. Will address sub texture blit in
        // future and remove these validations.
        if (source->origin.x != 0 || source->origin.y != 0 || source->origin.z != 0 ||
            destinationst->origin.x != 0 || destination->origin.y != 0 ||
            destination->origin.z != 0 || source->mipLevel != 0 || destination->mipLevel != 0 ||
            source->texture->GetSize() != destination->texture->GetSize()) {
            return DAWN_VALIDATION_ERROR("Cannot support sub blit now.");
        }

        return {};
    }

    MaybeError BlitTextureForBrowserHelper::DoBlitTextureForBrowser(
        const TextureCopyView* source,
        const TextureCopyView* destination,
        const Extent3D* copySize) {
        // TODO(shaobo.yan@intel.com): In D3D12 and Vulkan, compatible texture format can directly
        // copy to each other. This can be a potential fast path.

        // Get pre-built render pipeline
        InternalRenderPipelineType pipelineType =
            GetInternalRenderPipelineTypeForCopyTextureToTextureDawn(source, destination);
        RenderPipelineBase* pipeline = GetDevice()->GetInternalRenderPipeline(pipelineType);

        const RenderPipelineBase* GetBlitTextureForBrowserPipeline(
            wgpu::TextureDimension srcDim, wgpu::TextureFormat srcFormat,
            wgpu::TextureDimension dstDim, wgpu::TextureFormat dstFormat);

        RenderPipelineBase* pipeline =
            GetDevice()->GetInternalPipelineStore()->GetBlitTextureForBrowserPipeline(
                source->texture->GetDimension(), source->texture->GetFormat().format,
                destination->texture->GetDimension(), destination->texture->GetFormat().format);

        SamplerDescriptor samplerDesc = {};
        samplerDesc.minFilter = wgpu::FilterMode::Linear;
        samplerDesc.magFilter = wgpu::FilterMode::Linear;

        TextureViewDescriptor srcTextureViewDesc = {};
        srcTextureViewDesc.format = source->texture->GetFormat().format;
        srcTextureViewDesc.baseMipLevel = source->mipLevel;
        srcTextureViewDesc.mipLevelCount = 1;

        TextureViewBase* srcTextureView = source->texture->CreateView(&srcTextureViewDesc);

        // TODO(shaobo.yan@intel.com): rotation uniform value should be calculate to handle
        // rotation.
        const float rotationMatrix[] = {
            1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
        };

        GetDevice()->GetDefaultQueue()->WriteBuffer(mRotationUniform, 0, rotationMatrix,
                                                    sizeof(rotationMatrix));

        BindGroupDescriptor bglDesc = {};
        BindGroupEntry bindGroupEntries[3];
        bindGroupEntries[0].buffer = mRotationUniform.Get();
        bindGroupEntries[1].sampler = GetDevice()->CreateSampler(&samplerDesc);
        bindGroupEntries[2].textureView = srcTextureView;

        bglDesc.layout = pipeline->GetBindGroupLayout(0);
        bglDesc.entryCount = 2;
        bglDesc.entries = &bindGroupEntries[0];

        BindGroupBase* bindGroup = GetDevice()->CreateBindGroup(&bglDesc);

        CommandEncoderDescriptor encoderDesc = {};

        CommandEncoder* encoder = GetDevice()->CreateCommandEncoder(&encoderDesc);
        TextureViewDescriptor dstTextureViewDesc;
        dstTextureViewDesc.format = destination->texture->GetFormat().format;
        dstTextureViewDesc.baseMipLevel = destination->mipLevel;
        dstTextureViewDesc.mipLevelCount = 1;

        TextureViewBase* dstView = destination->texture->CreateView(&dstTextureViewDesc);
        RenderPassColorAttachmentDescriptor colorAttachmentDesc;
        colorAttachmentDesc.attachment = dstView;
        colorAttachmentDesc.clearColor = {0.0, 0.0, 0.0, 1.0};
        RenderPassDescriptor renderPassDesc;
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachmentDesc;
        RenderPassEncoder* passEncoder = encoder->BeginRenderPass(&renderPassDesc);
        passEncoder->SetPipeline(pipeline);

        // It's internal pipeline, we know the slot info.
        passEncoder->SetVertexBuffer(0, mVertexBuffer.Get(), 0, 0);
        passEncoder->SetBindGroup(0, bindGroup, 0, nullptr);
        passEncoder->Draw(6, 1, 0, 0);
        passEncoder->EndPass();

        CommandBufferDescriptor cbDesc = {};
        CommandBufferBase* commandBuffer = encoder->Finish(&cbDesc);

        GetDevice()->GetDefaultQueue()->Submit(1, &commandBuffer);

        return {};
    }

}  // namespace dawn_native
