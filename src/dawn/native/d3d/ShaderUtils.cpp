// Copyright 2023 The Dawn Authors
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

#include "dawn/native/d3d/ShaderUtils.h"

#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/common/BitSetIterator.h"
#include "dawn/common/Log.h"
#include "dawn/common/WindowsUtils.h"
#include "dawn/native/CacheKey.h"
#include "dawn/native/Pipeline.h"
#include "dawn/native/TintUtils.h"
#include "dawn/native/d3d/BlobD3D.h"
#include "dawn/native/d3d/D3DCompilationRequest.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d12/AdapterD3D12.h"
#include "dawn/native/d3d12/BackendD3D12.h"
#include "dawn/native/d3d12/BindGroupLayoutD3D12.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/PipelineLayoutD3D12.h"
#include "dawn/native/d3d12/PlatformFunctionsD3D12.h"
#include "dawn/native/d3d12/UtilsD3D12.h"
#include "dawn/native/stream/BlobSource.h"
#include "dawn/native/stream/ByteVectorSink.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

#include "tint/tint.h"

namespace dawn::native::d3d {

namespace {

std::vector<const wchar_t*> GetDXCArguments(uint32_t compileFlags, bool enable16BitTypes) {
    std::vector<const wchar_t*> arguments;
    if (compileFlags & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY) {
        arguments.push_back(L"/Gec");
    }
    if (compileFlags & D3DCOMPILE_IEEE_STRICTNESS) {
        arguments.push_back(L"/Gis");
    }
    constexpr uint32_t d3dCompileFlagsBits = D3DCOMPILE_OPTIMIZATION_LEVEL2;
    if (compileFlags & d3dCompileFlagsBits) {
        switch (compileFlags & D3DCOMPILE_OPTIMIZATION_LEVEL2) {
            case D3DCOMPILE_OPTIMIZATION_LEVEL0:
                arguments.push_back(L"/O0");
                break;
            case D3DCOMPILE_OPTIMIZATION_LEVEL2:
                arguments.push_back(L"/O2");
                break;
            case D3DCOMPILE_OPTIMIZATION_LEVEL3:
                arguments.push_back(L"/O3");
                break;
        }
    }
    if (compileFlags & D3DCOMPILE_DEBUG) {
        arguments.push_back(L"/Zi");
    }
    if (compileFlags & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR) {
        arguments.push_back(L"/Zpr");
    }
    if (compileFlags & D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR) {
        arguments.push_back(L"/Zpc");
    }
    if (compileFlags & D3DCOMPILE_AVOID_FLOW_CONTROL) {
        arguments.push_back(L"/Gfa");
    }
    if (compileFlags & D3DCOMPILE_PREFER_FLOW_CONTROL) {
        arguments.push_back(L"/Gfp");
    }
    if (compileFlags & D3DCOMPILE_RESOURCES_MAY_ALIAS) {
        arguments.push_back(L"/res_may_alias");
    }

    if (enable16BitTypes) {
        // enable-16bit-types are only allowed in -HV 2018 (default)
        arguments.push_back(L"/enable-16bit-types");
    }

    arguments.push_back(L"-HV");
    arguments.push_back(L"2018");

    return arguments;
}

ResultOrError<ComPtr<IDxcBlob>> CompileShaderDXC(const d3d::D3DBytecodeCompilationRequest& r,
                                                 const std::string& entryPointName,
                                                 const std::string& hlslSource) {
    ComPtr<IDxcBlobEncoding> sourceBlob;
    DAWN_TRY(CheckHRESULT(r.dxcLibrary->CreateBlobWithEncodingFromPinned(
                              hlslSource.c_str(), hlslSource.length(), CP_UTF8, &sourceBlob),
                          "DXC create blob"));

    std::wstring entryPointW;
    DAWN_TRY_ASSIGN(entryPointW, d3d::ConvertStringToWstring(entryPointName));

    std::vector<const wchar_t*> arguments = GetDXCArguments(r.compileFlags, r.hasShaderF16Feature);

    ComPtr<IDxcOperationResult> result;
    DAWN_TRY(CheckHRESULT(r.dxcCompiler->Compile(sourceBlob.Get(), nullptr, entryPointW.c_str(),
                                                 r.dxcShaderProfile.data(), arguments.data(),
                                                 arguments.size(), nullptr, 0, nullptr, &result),
                          "DXC compile"));

    HRESULT hr;
    DAWN_TRY(CheckHRESULT(result->GetStatus(&hr), "DXC get status"));

    if (FAILED(hr)) {
        ComPtr<IDxcBlobEncoding> errors;
        DAWN_TRY(CheckHRESULT(result->GetErrorBuffer(&errors), "DXC get error buffer"));

        return DAWN_VALIDATION_ERROR("DXC compile failed with: %s",
                                     static_cast<char*>(errors->GetBufferPointer()));
    }

    ComPtr<IDxcBlob> compiledShader;
    DAWN_TRY(CheckHRESULT(result->GetResult(&compiledShader), "DXC get result"));
    return std::move(compiledShader);
}

ResultOrError<ComPtr<ID3DBlob>> CompileShaderFXC(const d3d::D3DBytecodeCompilationRequest& r,
                                                 const std::string& entryPointName,
                                                 const std::string& hlslSource) {
    ComPtr<ID3DBlob> compiledShader;
    ComPtr<ID3DBlob> errors;

    DAWN_INVALID_IF(FAILED(r.d3dCompile(hlslSource.c_str(), hlslSource.length(), nullptr, nullptr,
                                        nullptr, entryPointName.c_str(), r.fxcShaderProfile.data(),
                                        r.compileFlags, 0, &compiledShader, &errors)),
                    "D3D compile failed with: %s", static_cast<char*>(errors->GetBufferPointer()));

    return std::move(compiledShader);
}

ResultOrError<std::string> TranslateToHLSL(
    d3d::HlslCompilationRequest r,
    CacheKey::UnsafeUnkeyedValue<dawn::platform::Platform*> tracePlatform,
    std::string* remappedEntryPointName,
    bool* usesVertexOrInstanceIndex) {
    std::ostringstream errorStream;
    errorStream << "Tint HLSL failure:" << std::endl;

    tint::transform::Manager transformManager;
    tint::transform::DataMap transformInputs;

    // Run before the renamer so that the entry point name matches `entryPointName` still.
    transformManager.Add<tint::transform::SingleEntryPoint>();
    transformInputs.Add<tint::transform::SingleEntryPoint::Config>(r.entryPointName.data());

    // Needs to run before all other transforms so that they can use builtin names safely.
    transformManager.Add<tint::transform::Renamer>();
    if (r.disableSymbolRenaming) {
        // We still need to rename HLSL reserved keywords
        transformInputs.Add<tint::transform::Renamer::Config>(
            tint::transform::Renamer::Target::kHlslKeywords);
    }

    if (r.stage == SingleShaderStage::Vertex) {
        transformManager.Add<tint::transform::FirstIndexOffset>();
        transformInputs.Add<tint::transform::FirstIndexOffset::BindingPoint>(
            r.firstIndexOffsetShaderRegister, r.firstIndexOffsetRegisterSpace);
    }

    if (r.substituteOverrideConfig) {
        // This needs to run after SingleEntryPoint transform which removes unused overrides for
        // current entry point.
        transformManager.Add<tint::transform::SubstituteOverride>();
        transformInputs.Add<tint::transform::SubstituteOverride::Config>(
            std::move(r.substituteOverrideConfig).value());
    }

    tint::Program transformedProgram;
    tint::transform::DataMap transformOutputs;
    {
        TRACE_EVENT0(tracePlatform.UnsafeGetValue(), General, "RunTransforms");
        DAWN_TRY_ASSIGN(transformedProgram,
                        RunTransforms(&transformManager, r.inputProgram, transformInputs,
                                      &transformOutputs, nullptr));
    }

    if (auto* data = transformOutputs.Get<tint::transform::Renamer::Data>()) {
        auto it = data->remappings.find(r.entryPointName.data());
        if (it != data->remappings.end()) {
            *remappedEntryPointName = it->second;
        } else {
            DAWN_INVALID_IF(!r.disableSymbolRenaming,
                            "Could not find remapped name for entry point.");

            *remappedEntryPointName = r.entryPointName;
        }
    } else {
        return DAWN_VALIDATION_ERROR("Transform output missing renamer data.");
    }

    if (r.stage == SingleShaderStage::Compute) {
        // Validate workgroup size after program runs transforms.
        Extent3D _;
        DAWN_TRY_ASSIGN(_, ValidateComputeStageWorkgroupSize(
                               transformedProgram, remappedEntryPointName->data(), r.limits));
    }

    if (r.stage == SingleShaderStage::Vertex) {
        if (auto* data = transformOutputs.Get<tint::transform::FirstIndexOffset::Data>()) {
            *usesVertexOrInstanceIndex = data->has_vertex_or_instance_index;
        } else {
            return DAWN_VALIDATION_ERROR("Transform output missing first index offset data.");
        }
    }

    tint::writer::hlsl::Options options;
    options.disable_robustness = !r.isRobustnessEnabled;
    options.disable_workgroup_init = r.disableWorkgroupInit;
    options.binding_remapper_options = r.bindingRemapper;
    options.external_texture_options = r.externalTextureOptions;

    if (r.usesNumWorkgroups) {
        options.root_constant_binding_point =
            tint::writer::BindingPoint{r.numWorkgroupsRegisterSpace, r.numWorkgroupsShaderRegister};
    }
    // TODO(dawn:549): HLSL generation outputs the indices into the
    // array_length_from_uniform buffer that were actually used. When the blob cache can
    // store more than compiled shaders, we should reflect these used indices and store
    // them as well. This would allow us to only upload root constants that are actually
    // read by the shader.
    options.array_length_from_uniform = r.arrayLengthFromUniform;

    if (r.stage == SingleShaderStage::Vertex) {
        // Now that only vertex shader can have interstage outputs.
        // Pass in the actually used interstage locations for tint to potentially truncate unused
        // outputs.
        options.interstage_locations = r.interstageLocations;
    }

    options.polyfill_reflect_vec2_f32 = r.polyfillReflectVec2F32;

    TRACE_EVENT0(tracePlatform.UnsafeGetValue(), General, "tint::writer::hlsl::Generate");
    auto result = tint::writer::hlsl::Generate(&transformedProgram, options);
    DAWN_INVALID_IF(!result.success, "An error occured while generating HLSL: %s", result.error);

    return std::move(result.hlsl);
}

}  // anonymous namespace

// D3D12_SHADER_BYTECODE CompiledShader::GetD3D12ShaderBytecode() const {
//     return {shaderBlob.Data(), shaderBlob.Size()};
// }

ResultOrError<CompiledShader> CompileShader(d3d::D3DCompilationRequest r) {
    CompiledShader compiledShader;
    // Compile the source shader to HLSL.
    std::string remappedEntryPoint;
    DAWN_TRY_ASSIGN(compiledShader.hlslSource,
                    TranslateToHLSL(std::move(r.hlsl), r.tracePlatform, &remappedEntryPoint,
                                    &compiledShader.usesVertexOrInstanceIndex));

    switch (r.bytecode.compiler) {
        case d3d::Compiler::DXC: {
            TRACE_EVENT0(r.tracePlatform.UnsafeGetValue(), General, "CompileShaderDXC");
            ComPtr<IDxcBlob> compiledDXCShader;
            DAWN_TRY_ASSIGN(compiledDXCShader, CompileShaderDXC(r.bytecode, remappedEntryPoint,
                                                                compiledShader.hlslSource));
            compiledShader.shaderBlob = CreateBlob(std::move(compiledDXCShader));
            break;
        }
        case d3d::Compiler::FXC: {
            TRACE_EVENT0(r.tracePlatform.UnsafeGetValue(), General, "CompileShaderFXC");
            ComPtr<ID3DBlob> compiledFXCShader;
            DAWN_TRY_ASSIGN(compiledFXCShader, CompileShaderFXC(r.bytecode, remappedEntryPoint,
                                                                compiledShader.hlslSource));
            compiledShader.shaderBlob = CreateBlob(std::move(compiledFXCShader));
            break;
        }
    }

    // If dumpShaders is false, we don't need the HLSL for logging. Clear the contents so it
    // isn't stored into the cache.
    if (!r.hlsl.dumpShaders) {
        compiledShader.hlslSource = "";
    }
    return compiledShader;
}

}  // namespace dawn::native::d3d
