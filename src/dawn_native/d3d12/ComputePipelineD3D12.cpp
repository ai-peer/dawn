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

#include "dawn_native/d3d12/ComputePipelineD3D12.h"

#include "common/Assert.h"
#include "common/HashUtils.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/PipelineCacheD3D12.h"
#include "dawn_native/d3d12/PipelineLayoutD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"
#include "dawn_native/d3d12/ShaderModuleD3D12.h"
#include "dawn_native/d3d12/UtilsD3D12.h"

namespace dawn_native { namespace d3d12 {

    ResultOrError<ComputePipeline*> ComputePipeline::Create(
        Device* device,
        const ComputePipelineDescriptor* descriptor) {
        Ref<ComputePipeline> pipeline = AcquireRef(new ComputePipeline(device, descriptor));
        DAWN_TRY(pipeline->Initialize(descriptor));
        return pipeline.Detach();
    }

    MaybeError ComputePipeline::Initialize(const ComputePipelineDescriptor* descriptor) {
        Device* device = ToBackend(GetDevice());
        uint32_t compileFlags = 0;
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        // SPRIV-cross does matrix multiplication expecting row major matrices
        compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

        ShaderModule* module = ToBackend(descriptor->computeStage.module);

        D3D12_COMPUTE_PIPELINE_STATE_DESC d3dDesc = {};

        ComPtr<IDxcBlob> compiledDXCShader;
        ComPtr<ID3DBlob> compiledFXCShader;

        std::unique_ptr<uint8_t[]> shaderBlob;

        bool useCachedShaders = device->IsToggleEnabled(Toggle::UseD3D12ShaderCaching);

        PersistentCacheKey shaderBlobKey = {};
        if (useCachedShaders) {
            // Compute the shader key using the hash.
            size_t shaderHash = ShaderModuleBase::HashForCache(module, false);

            // If the source contains multiple entry points, we must ensure they are
            // cached seperately per stage since DX shader code can only be compiled per stage
            // using the same entry point.
            HashCombine(&shaderHash, SingleShaderStage::Compute);
            HashCombine(&shaderHash, descriptor->computeStage.entryPoint);

            shaderBlobKey = PersistentCache::CreateKey(shaderHash);

            // Load the shader blob from the cache.
            const size_t shaderBlobSize = device->GetPersistentCache()->getDataSize(shaderBlobKey);
            if (shaderBlobSize > 0) {
                shaderBlob.reset(new uint8_t[shaderBlobSize]);
                device->GetPersistentCache()->loadData(shaderBlobKey, shaderBlob.get(),
                                                       shaderBlobSize);

                d3dDesc.CS.pShaderBytecode = shaderBlob.get();
                d3dDesc.CS.BytecodeLength = shaderBlobSize;
            }
        }

        if (d3dDesc.CS.pShaderBytecode == nullptr) {
            // Note that the HLSL will always use entryPoint "main".
            std::string hlslSource;
            DAWN_TRY_ASSIGN(hlslSource, module->TranslateToHLSL(descriptor->computeStage.entryPoint,
                                                                SingleShaderStage::Compute,
                                                                ToBackend(GetLayout())));

            if (device->IsToggleEnabled(Toggle::UseDXC)) {
                DAWN_TRY_ASSIGN(compiledDXCShader,
                                CompileShaderDXC(device, SingleShaderStage::Compute, hlslSource,
                                                 "main", compileFlags));

                d3dDesc.CS.pShaderBytecode = compiledDXCShader->GetBufferPointer();
                d3dDesc.CS.BytecodeLength = compiledDXCShader->GetBufferSize();
            } else {
                DAWN_TRY_ASSIGN(compiledFXCShader,
                                CompileShaderFXC(device, SingleShaderStage::Compute, hlslSource,
                                                 "main", compileFlags));
                d3dDesc.CS.pShaderBytecode = compiledFXCShader->GetBufferPointer();
                d3dDesc.CS.BytecodeLength = compiledFXCShader->GetBufferSize();
            }

            // Store shader in cache.
            if (useCachedShaders) {
                useCachedShaders = device->GetPersistentCache()->storeData(
                    shaderBlobKey, d3dDesc.CS.pShaderBytecode, d3dDesc.CS.BytecodeLength);
            }
        }

        d3dDesc.pRootSignature = ToBackend(GetLayout())->GetRootSignature();

        // D3D compiler debug flag will add new metadata to the complied DX shader.
        // This will cause PSO load to fail since we lookup the PSO from the source.
        // This means DX shaders must also be cached to use PSO caching in debug builds.
        const bool usePipelineCache = useCachedShaders || ((compileFlags & D3DCOMPILE_DEBUG) == 0);

        // Create a new PSO or re-use a prebuilt one from the pipeline cache.
        size_t psoKey = 0;
        if (usePipelineCache) {
            psoKey = HashForCache(this, false);
            DAWN_TRY_ASSIGN(mPipelineState,
                            device->GetPipelineCache()->loadComputePipeline(d3dDesc, psoKey));
        }

        if (mPipelineState == nullptr) {
            DAWN_TRY(CheckHRESULT(device->GetD3D12Device()->CreateComputePipelineState(
                                      &d3dDesc, IID_PPV_ARGS(&mPipelineState)),
                                  "ID3D12Device::CreateComputePipelineState"));
            if (usePipelineCache) {
                DAWN_TRY(device->GetPipelineCache()->storePipeline(mPipelineState.Get(), psoKey));
            }
        }

        return {};
    }

    ComputePipeline::~ComputePipeline() {
        ToBackend(GetDevice())->ReferenceUntilUnused(mPipelineState);
    }

    ID3D12PipelineState* ComputePipeline::GetPipelineState() const {
        return mPipelineState.Get();
    }

}}  // namespace dawn_native::d3d12
