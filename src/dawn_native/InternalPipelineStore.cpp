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

#include "dawn_native/InternalPipelineStore.h"

#include "common/Assert.h"
#include "dawn_native/Device.h"
#include "dawn_native/RenderPipeline.h"
#include "dawn_native/internal_pipelines/BlitTextureForBrowserPipelineInfo.h"

// Shader Source
#include "dawn_native/internal_pipelines/shaders/BlitTextureVertexWGSL.h"
#include "dawn_native/internal_pipelines/shaders/Passthrough4Channel2DTexturesFragmentWGSL.h"

namespace dawn_native {
    namespace {
        const ShaderModuleWGSLDescriptor GetShaderModuleWGSLDesc(InternalShaderType type) {
            ShaderModuleWGSLDescriptor wgslDesc = {};

            switch (type) {
                case InternalShaderType::BlitTextureVertex:
                    wgslDesc.source = g_blit_texture_vertex;
                    break;
                case InternalShaderType::Passthrough4Channel2DTextureFragment:
                    wgslDesc.source = g_passthrough_2d_4_channel_frag;
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
            return wgslDesc;
        }

        BaseRenderPipelineInfo* GetInternalRenderPipelineInfo(InternalRenderPipelineType type) {
            switch (type) {
                case InternalRenderPipelineType::BlitWithRotation:
                    return new BlitWithRotationPipelineInfo();
                default:
                    UNREACHABLE();
                    return new BaseRenderPipelineInfo();
            }
        }
    }  // anonymous namespace

    InternalPipelineStore::InternalPipelineStore(DeviceBase* device) : mDevice(device) {
        // Load all internal shaders and create shader module
        for (InternalShaderType shader : kAllInternalShaders) {
            ShaderModuleDescriptor descriptor;
            ShaderModuleWGSLDescriptor wgslDesc = GetShaderModuleWGSLDesc(shader);
            descriptor.nextInChain = reinterpret_cast<ChainedStruct*>(&wgslDesc);

            uint32_t key = static_cast<uint32_t>(shader);
            mInternalShaderModuleCache[key] = mDevice->CreateShaderModule(&descriptor);
        }

        // Intenral shader use fixed entries.
        char vertexShaderEntry[] = "vertex_main";
        char fragmentShaderEntry[] = "fragment_main";

        for (InternalRenderPipelineType pipeline : kAllInternalRenderPipelines) {
            BaseRenderPipelineInfo* info = GetInternalRenderPipelineInfo(pipeline);
            RenderPipelineDescriptor* descriptor = info;
            uint32_t vertexKey = static_cast<uint32_t>(info->vertexType);
            descriptor->vertexStage.module = mInternalShaderModuleCache[vertexKey];
            descriptor->vertexStage.entryPoint = vertexShaderEntry;
            ProgrammableStageDescriptor fragmentDesc = {};
            fragmentDesc.entryPoint = fragmentShaderEntry;
            uint32_t fragmentKey = static_cast<uint32_t>(info->fragType);
            fragmentDesc.module = mInternalShaderModuleCache[fragmentKey];
            descriptor->fragmentStage = &fragmentDesc;

            uint32_t key = static_cast<uint32_t>(pipeline);
            mInternalRenderPipelineCache[key] = mDevice->CreateRenderPipeline(descriptor);
        }
    }

    InternalPipelineStore::~InternalPipelineStore() {
        // Destroy cached pipelines.
        for (uint32_t i = 0; i < mInternalRenderPipelineCache.size(); ++i) {
            mInternalRenderPipelineCache[i]->Release();
        }

        // Destroy cached shader modules.
        for (uint32_t i = 0; i < mInternalShaderModuleCache.size(); ++i) {
            mInternalShaderModuleCache[i]->Release();
        }
    }

    RenderPipelineBase* InternalPipelineStore::GetBlitTextureForBrowserPipeline(
        wgpu::TextureDimension srcDim,
        wgpu::TextureFormat srcFormat,
        wgpu::TextureDimension dstDim,
        wgpu::TextureFormat dstFormat) {
        if (srcDim == wgpu::TextureDimension::e2D) {
            if (dstDim == wgpu::TextureDimension::e2D) {
                if (srcFormat == wgpu::TextureFormat::RGBA8Unorm) {
                    switch (dstFormat) {
                        case wgpu::TextureFormat::RGBA8Unorm: {
                            uint32_t key =
                                static_cast<uint32_t>(InternalRenderPipelineType::BlitWithRotation);
                            return mInternalRenderPipelineCache[key];
                        }
                        default: {
                            UNREACHABLE();
                        }
                    }
                }
            }
        }
        return nullptr;
    }

}  // namespace dawn_native