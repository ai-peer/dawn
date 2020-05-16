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
#include "common/Log.h"
#include "dawn_native/d3d12/D3D12Error.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
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
        std::string hlslSource;
        DAWN_TRY_ASSIGN(hlslSource, module->GetHLSLSource(ToBackend(GetLayout())));

        D3D12_COMPUTE_PIPELINE_STATE_DESC d3dDesc = {};
        d3dDesc.pRootSignature = ToBackend(GetLayout())->GetRootSignature();

        ComPtr<IDxcBlob> compiledDXCShader;
        ComPtr<ID3DBlob> compiledFXCShader;

        if (device->IsToggleEnabled(Toggle::UseDXC)) {
            IDxcLibrary* dxcLibrary;
            DAWN_TRY_ASSIGN(dxcLibrary, ToBackend(GetDevice())->GetOrCreateDxcLibrary());
            IDxcCompiler* dxcCompiler;
            DAWN_TRY_ASSIGN(dxcCompiler, ToBackend(GetDevice())->GetOrCreateDxcCompiler());

            std::wstring entryPoint = ConvertStringToWstring(descriptor->computeStage.entryPoint);
            std::wstring compileTarget = L"cs_6_0";

            ComPtr<IDxcBlobEncoding> sourceBlob;
            if (FAILED(dxcLibrary->CreateBlobWithEncodingOnHeapCopy(
                    hlslSource.c_str(), hlslSource.length(), CP_ACP, &sourceBlob))) {
            }

            std::vector<LPCWSTR> arguments = module->GetDXCArguments(compileFlags);

            ComPtr<IDxcOperationResult> result;
            if (FAILED(dxcCompiler->Compile(sourceBlob.Get(),       // pSource
                                            nullptr,                // pSourceName
                                            entryPoint.c_str(),     // pEntryPoint
                                            compileTarget.c_str(),  // pTargetProfile
                                            arguments.data(),
                                            arguments.size(),  // pArguments, argCount
                                            nullptr, 0,        // pDefines, defineCount
                                            nullptr,           // pIncludeHandler
                                            &result)))         // ppResult
            {
            }

            ComPtr<IDxcBlobEncoding> errors;

            HRESULT hr;
            result->GetStatus(&hr);
            MaybeError error = CheckHRESULT(hr, "D3DCompile");
            if (error.IsError()) {
                if (result) {
                    if (SUCCEEDED(result->GetErrorBuffer(&errors))) {
                        dawn::WarningLog() << reinterpret_cast<char*>(errors->GetBufferPointer());
                        DAWN_TRY(std::move(error));
                    }
                }
            }

            result->GetResult(&compiledDXCShader);

            d3dDesc.CS.pShaderBytecode = compiledDXCShader->GetBufferPointer();
            d3dDesc.CS.BytecodeLength = compiledDXCShader->GetBufferSize();
        } else {
            ComPtr<ID3DBlob> errors;

            const PlatformFunctions* functions = device->GetFunctions();
            if (FAILED(functions->d3dCompile(hlslSource.c_str(), hlslSource.length(), nullptr,
                                             nullptr, nullptr, descriptor->computeStage.entryPoint,
                                             "cs_5_1", compileFlags, 0, &compiledFXCShader,
                                             &errors))) {
                dawn::WarningLog() << reinterpret_cast<char*>(errors->GetBufferPointer());
                ASSERT(false);
            }

            d3dDesc.CS.pShaderBytecode = compiledFXCShader->GetBufferPointer();
            d3dDesc.CS.BytecodeLength = compiledFXCShader->GetBufferSize();
        }

        device->GetD3D12Device()->CreateComputePipelineState(&d3dDesc,
                                                             IID_PPV_ARGS(&mPipelineState));
        return {};
    }

    ComputePipeline::~ComputePipeline() {
        ToBackend(GetDevice())->ReferenceUntilUnused(mPipelineState);
    }

    ID3D12PipelineState* ComputePipeline::GetPipelineState() const {
        return mPipelineState.Get();
    }

}}  // namespace dawn_native::d3d12
