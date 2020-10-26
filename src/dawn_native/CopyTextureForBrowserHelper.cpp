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

#include "dawn_native/CopyTextureForBrowserHelper.h"

#include "dawn_native/BindGroup.h"
#include "dawn_native/BindGroupLayout.h"
#include "dawn_native/CommandBuffer.h"
#include "dawn_native/CommandEncoder.h"
#include "dawn_native/CommandValidation.h"
#include "dawn_native/Device.h"
#include "dawn_native/InternalPipelineStore.h"
#include "dawn_native/Queue.h"
#include "dawn_native/RenderPassEncoder.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/Sampler.h"

#include <unordered_set>

namespace dawn_native {
    namespace {

        // TODO(shaobo.yan@intel.com): Expand supported blit texture formats
        const std::unordered_set<wgpu::TextureFormat> validBlitSrcTextureFormats = {
            wgpu::TextureFormat::RGBA8Unorm};
        const std::unordered_set<wgpu::TextureFormat> validBlitDstTextureFormats = {
            wgpu::TextureFormat::RGBA8Unorm};

        static const char g_copy_texture_for_browser_vertex[] = R"(
            [[block]] struct Uniforms {
                [[offset(0)]] rotation : mat4x4<f32>;
            };
            const pos : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
                vec2<f32>(1.0, 1.0),
                vec2<f32>(1.0, -1.0),
                vec2<f32>(-1.0, -1.0),
                vec2<f32>(1.0, 1.0),
                vec2<f32>(-1.0, -1.0),
                vec2<f32>(-1.0, 1.0));

            const texCoord : array<vec2<f32>, 6> = array<vec2<f32>, 6>(
                vec2<f32>(1.0, 0.0),
                vec2<f32>(1.0, 1.0),
                vec2<f32>(0.0, 1.0),
                vec2<f32>(1.0, 0.0),
                vec2<f32>(0.0, 1.0),
                vec2<f32>(0.0, 0.0));

            [[location(0)]] var<out> fragUV: vec2<f32>;
            [[builtin(position)]] var<out> Position : vec4<f32>;
            [[builtin(vertex_idx)]] var<in> VertexIndex : i32;
            [[binding(0), set(0)]] var<uniform> uniforms : Uniforms;
            [[stage(vertex)]]
            fn vertex_main() -> void {
                Position = uniforms.rotation * vec4<f32>(pos[VertexIndex], 0.0, 1.0);
                fragUV = texCoord[VertexIndex];
                return;
            }
        )";

        static const char g_passthrough_2d_4_channel_frag[] = R"(
            [[binding(1), set(0)]] var<uniform_constant> mySampler: sampler;
            [[binding(2), set(0)]] var<uniform_constant> myTexture: texture_sampled_2d<f32>;
            [[location(0)]] var<in> fragUV : vec2<f32>;
            [[location(0)]] var<out> rgbaColor : vec4<f32>;
            [[stage(fragment)]]
            fn fragment_main() -> void {
                rgbaColor = textureSample(myTexture, mySampler, fragUV);
                return;
            }
        )";

        enum class CopyTextureForBrowserFragmentType : uint32_t {
            Passthrough4Channel2DTextureFragment,
        };

        enum class CopyTextureForBrowserPipelineType : uint32_t {
            HandleRotation,
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

        uint32_t GetCopyTextureForBrowserPipelineKey() {
            // Will have more pipeline to handle different cases in future.
            return static_cast<uint32_t>(CopyTextureForBrowserPipelineType::HandleRotation);
        }

        CopyTextureForBrowserFragmentType GetCopyTextureForBrowserFragmentType() {
            // Will have more fragment shader to handle format convert in future.
            return CopyTextureForBrowserFragmentType::Passthrough4Channel2DTextureFragment;
        }

        ShaderModuleWGSLDescriptor GetShaderModuleWGSLDesc(CopyTextureForBrowserFragmentType type) {
            // Will have more fragment shader to handle format convert in future.
            ShaderModuleWGSLDescriptor wgslDesc = {};
            wgslDesc.source = g_passthrough_2d_4_channel_frag;
            return wgslDesc;
        }
    }  // anonymous namespace

    MaybeError ValidateCopyTextureForBrowser(DeviceBase* device,
                                             const TextureCopyView* source,
                                             const TextureCopyView* destination,
                                             const Extent3D* copySize) {
        DAWN_TRY(device->ValidateObject(source->texture));
        DAWN_TRY(device->ValidateObject(destination->texture));

        DAWN_TRY(ValidateTextureCopyView(device, *source, *copySize));
        DAWN_TRY(ValidateTextureCopyView(device, *destination, *copySize));

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

    MaybeError DoCopyTextureForBrowser(DeviceBase* device,
                                       const TextureCopyView* source,
                                       const TextureCopyView* destination,
                                       const Extent3D* copySize) {
        // TODO(shaobo.yan@intel.com): In D3D12 and Vulkan, compatible texture format can directly
        // copy to each other. This can be a potential fast path.

        uint32_t pipelineKey = GetCopyTextureForBrowserPipelineKey();

        InternalPipelineStore* store = device->GetInternalPipelineStore();

        // Create and register pipeline if not cached before.
        if (store->CopyTextureForBrowserPipeline.find(pipelineKey) ==
            store->CopyTextureForBrowserPipeline.end()) {
            // Create vertex shader module if not cached before.
            if (store->CopyTextureForBrowserVS == nullptr) {
                ShaderModuleDescriptor descriptor;
                ShaderModuleWGSLDescriptor wgslDesc;
                wgslDesc.source = g_copy_texture_for_browser_vertex;
                descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);

                store->CopyTextureForBrowserVS = device->CreateShaderModule(&descriptor);
            }

            ShaderModuleBase* vertexModule = store->CopyTextureForBrowserVS.Get();

            // Create fragment shader module if not cached before;
            CopyTextureForBrowserFragmentType fragmentType = GetCopyTextureForBrowserFragmentType();
            uint32_t fragmentkey = static_cast<uint32_t>(fragmentType);
            if (store->CopyTextureForBrowserFS.find(fragmentkey) ==
                store->CopyTextureForBrowserFS.end()) {
                ShaderModuleDescriptor descriptor;
                ShaderModuleWGSLDescriptor wgslDesc = GetShaderModuleWGSLDesc(fragmentType);
                descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);
                store->CopyTextureForBrowserFS.insert(
                    {fragmentkey, device->CreateShaderModule(&descriptor)});
            }

            ShaderModuleBase* fragmentModule = store->CopyTextureForBrowserFS[fragmentkey].Get();

            // Create and register the render pipeline.
            RenderPipelineDescriptor descriptor = {};
            ColorStateDescriptor cColorState;

            descriptor.primitiveTopology = wgpu::PrimitiveTopology::TriangleList;

            cColorState.format = wgpu::TextureFormat::RGBA8Unorm;
            descriptor.colorStateCount = 1;
            descriptor.colorStates = &cColorState;
            descriptor.depthStencilState = nullptr;
            descriptor.layout = nullptr;

            descriptor.vertexStage.module = vertexModule;
            descriptor.vertexStage.entryPoint = "vertex_main";
            ProgrammableStageDescriptor fragmentDesc = {};
            fragmentDesc.entryPoint = "fragment_main";
            fragmentDesc.module = fragmentModule;
            descriptor.fragmentStage = &fragmentDesc;

            store->CopyTextureForBrowserPipeline.insert(
                {pipelineKey, device->CreateRenderPipeline(&descriptor)});
        }

        RenderPipelineBase* pipeline = store->CopyTextureForBrowserPipeline[pipelineKey].Get();

        // Use default configure, filterMode set to Nearest for min and mag.
        SamplerDescriptor samplerDesc = {};
        SamplerBase* sampler = device->CreateSampler(&samplerDesc);

        TextureViewDescriptor srcTextureViewDesc = {};
        srcTextureViewDesc.format = source->texture->GetFormat().format;
        srcTextureViewDesc.baseMipLevel = source->mipLevel;
        srcTextureViewDesc.mipLevelCount = 1;

        TextureViewBase* srcTextureView = source->texture->CreateView(&srcTextureViewDesc);

        // TODO(shaobo.yan@intel.com): rotation uniform value should be calculate to handle
        // rotation.
        const float rotationMatrix[] = {
            1.0, 0.0, 0.0, 0.0,  //
            0.0, 1.0, 0.0, 0.0,  //
            0.0, 0.0, 1.0, 0.0,  //
            0.0, 0.0, 0.0, 1.0,  //
        };

        BufferDescriptor rotationUniformDesc = {};
        rotationUniformDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        rotationUniformDesc.size = 64;  // 2x2 matrix for rotation 2D texture
        BufferBase* rotationUniform = device->CreateBuffer(&rotationUniformDesc);

        device->GetDefaultQueue()->WriteBuffer(rotationUniform, 0, rotationMatrix,
                                               sizeof(rotationMatrix));

        BindGroupDescriptor bglDesc = {};
        BindGroupEntry bindGroupEntries[3] = {};
        bindGroupEntries[0].binding = 0;
        bindGroupEntries[0].buffer = rotationUniform;
        bindGroupEntries[0].size = sizeof(rotationMatrix);
        bindGroupEntries[1].binding = 1;
        bindGroupEntries[1].sampler = sampler;
        bindGroupEntries[2].binding = 2;
        bindGroupEntries[2].textureView = srcTextureView;

        BindGroupLayoutBase* layout = pipeline->GetBindGroupLayout(0);
        bglDesc.layout = layout;
        bglDesc.entryCount = 3;
        bglDesc.entries = &bindGroupEntries[0];

        BindGroupBase* bindGroup = device->CreateBindGroup(&bglDesc);

        CommandEncoderDescriptor encoderDesc = {};

        CommandEncoder* encoder = device->CreateCommandEncoder(&encoderDesc);
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
        passEncoder->SetBindGroup(0, bindGroup, 0, nullptr);
        passEncoder->Draw(6, 1, 0, 0);
        passEncoder->EndPass();

        CommandBufferDescriptor cbDesc = {};
        CommandBufferBase* commandBuffer = encoder->Finish(&cbDesc);

        device->GetDefaultQueue()->Submit(1, &commandBuffer);

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
