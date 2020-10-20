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

#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/CommandbUFFER.H"
#include "dawn_native/Device.h"
#include "dawn_native/InternalPipelineStore.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderPassEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/Sampler.h"
#include "dawn_native/internal_pipelines/InternalPipelineUtils.h"

#include <unordered_set>

namespace dawn_native {
    namespace {
        // TODO(shaobo.yan@intel.com): Expand supported blit texture formats
        static const std::unordered_set<wgpu::TextureFormat> validBlitSrcTextureFormats = {
            wgpu::TextureFormat::RGBA8Unorm};
        static const std::unordered_set<wgpu::TextureFormat> validBlitDstTextureFormats = {
            wgpu::TextureFormat::RGBA8Unorm};

        static const float rectVertices[] = {
            // position texCoord
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

    BlitTextureForBrowserHelper::BlitTextureForBrowserHelper(DeviceBase* device) : mDevice(device) {
        BufferDescriptor vertexBufferDesc = {};
        vertexBufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
        vertexBufferDesc.size = sizeof(rectVertices);
        mVertexBuffer = mDevice->CreateBuffer(&vertexBufferDesc);
        mDevice->GetDefaultQueue()->WriteBuffer(mVertexBuffer, 0, rectVertices,
                                                sizeof(rectVertices));

        BufferDescriptor rotationUniformDesc = {};
        rotationUniformDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        rotationUniformDesc.size = 64;  // 4x4 matrix for rotation
        mRotationUniform = mDevice->CreateBuffer(&rotationUniformDesc);
    }

    BlitTextureForBrowserHelper::~BlitTextureForBrowserHelper() {
        mDevice = nullptr;
        mVertexBuffer = nullptr;
        mRotationUniform = nullptr;
    }

    MaybeError BlitTextureForBrowserHelper::ValidateBlitForBrowser(
        const TextureCopyView* source,
        const TextureCopyView* destination,
        const Extent3D* copySize) {
        DAWN_TRY(mDevice->ValidateObject(source->texture));
        DAWN_TRY(mDevice->ValidateObject(destination->texture));

        DAWN_TRY(ValidateTextureCopyView(mDevice, *source, *copySize));
        DAWN_TRY(ValidateTextureCopyView(mDevice, *destination, *copySize));

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
            destination->origin.x != 0 || destination->origin.y != 0 ||
            destination->origin.z != 0 || source->mipLevel != 0 || destination->mipLevel != 0 ||
            source->texture->GetWidth() != destination->texture->GetWidth() ||
            source->texture->GetHeight() != destination->texture->GetHeight()) {
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
        RenderPipelineBase* pipeline =
            mDevice->GetInternalPipelineStore()->GetBlitTextureForBrowserPipeline(
                source->texture->GetDimension(), source->texture->GetFormat().format,
                destination->texture->GetDimension(), destination->texture->GetFormat().format);

        // Use default configure, filterMode set to Nearest for min and mag.
        SamplerDescriptor samplerDesc = {};
        SamplerBase* sampler = mDevice->CreateSampler(&samplerDesc);

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

        mDevice->GetDefaultQueue()->WriteBuffer(mRotationUniform, 0, rotationMatrix,
                                                sizeof(rotationMatrix));

        BindGroupDescriptor bglDesc = {};
        BindGroupEntry bindGroupEntries[3];
        bindGroupEntries[0].binding = 0;
        bindGroupEntries[0].buffer = mRotationUniform;
        bindGroupEntries[0].sampler = nullptr;
        bindGroupEntries[0].textureView = nullptr;
        bindGroupEntries[0].size = sizeof(rotationMatrix);
        bindGroupEntries[1].binding = 1;
        bindGroupEntries[1].sampler = sampler;
        bindGroupEntries[1].buffer = nullptr;
        bindGroupEntries[1].textureView = nullptr;
        bindGroupEntries[2].binding = 2;
        bindGroupEntries[2].textureView = srcTextureView;
        bindGroupEntries[2].buffer = nullptr;
        bindGroupEntries[2].sampler = nullptr;

        BindGroupLayoutBase* layout = pipeline->GetBindGroupLayout(0);
        bglDesc.layout = layout;
        bglDesc.entryCount = 3;
        bglDesc.entries = &bindGroupEntries[0];

        BindGroupBase* bindGroup = mDevice->CreateBindGroup(&bglDesc);

        CommandEncoderDescriptor encoderDesc = {};

        CommandEncoder* encoder = mDevice->CreateCommandEncoder(&encoderDesc);
        TextureViewDescriptor dstTextureViewDesc;
        dstTextureViewDesc.format = destination->texture->GetFormat().format;
        dstTextureViewDesc.baseMipLevel = destination->mipLevel;
        dstTextureViewDesc.mipLevelCount = 1;

        TextureViewBase* dstView = destination->texture->CreateView(&dstTextureViewDesc);
        RenderPassColorAttachmentDescriptor colorAttachmentDesc;
        colorAttachmentDesc.attachment = dstView;
        colorAttachmentDesc.resolveTarget = nullptr;
        colorAttachmentDesc.loadOp = wgpu::LoadOp::Clear;
        colorAttachmentDesc.storeOp = wgpu::StoreOp::Store;
        colorAttachmentDesc.clearColor = {0.0, 0.0, 0.0, 1.0};
        RenderPassDescriptor renderPassDesc;
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachmentDesc;
        renderPassDesc.occlusionQuerySet = nullptr;
        RenderPassEncoder* passEncoder = encoder->BeginRenderPass(&renderPassDesc);
        passEncoder->SetPipeline(pipeline);

        // It's internal pipeline, we know the slot info.
        passEncoder->SetVertexBuffer(0, mVertexBuffer, 0, 0);
        passEncoder->SetBindGroup(0, bindGroup, 0, nullptr);
        passEncoder->Draw(6, 1, 0, 0);
        passEncoder->EndPass();

        CommandBufferDescriptor cbDesc = {};
        CommandBufferBase* commandBuffer = encoder->Finish(&cbDesc);

        mDevice->GetDefaultQueue()->Submit(1, &commandBuffer);

        // Release all temp object to avoid memory leak.
        sampler->Release();
        layout->Release();
        bindGroup->Release();
        passEncoder->Release();
        encoder->Release();
        commandBuffer->Release();

        return {};
    }

}  // namespace dawn_native
