// Copyright 2021 The Dawn & Tint Authors
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

#include <string>

#include "src/tint/lang/hlsl/validate/validate.h"

#include "src/tint/utils/command/command.h"
#include "src/tint/utils/file/tmpfile.h"
#include "src/tint/utils/macros/defer.h"
#include "src/tint/utils/text/string.h"

#ifdef _WIN32
#include <Windows.h>
// fxc headers
#include <d3dcommon.h>
#include <d3dcompiler.h>

// dxc headers
#include <dxcapi.h>

#include <wrl.h>
using Microsoft::WRL::ComPtr;
#endif  // _WIN32

namespace {
std::wstring ascii_to_wstring(std::string_view s) {
    return std::wstring(s.begin(), s.end());
}
}  // namespace

namespace tint::hlsl::validate {

Result ValidateUsingDXC(const std::string& dxc_path,
                        const std::string& source,
                        const EntryPointList& entry_points,
                        bool require_16bit_types,
                        uint32_t hlsl_shader_model) {
    Result result;

    if (entry_points.empty()) {
        result.output = "No entrypoint found";
        result.failed = true;
        return result;
    }

    auto dxc = tint::Command(dxc_path);
    if (!dxc.Found()) {
        result.output = "DXC not found at '" + std::string(dxc_path) + "'";
        result.failed = true;
        return result;
    }

    // Native 16-bit types, e.g. float16_t, require SM6.2. Otherwise we use SM6.0.
    if (hlsl_shader_model < 60 || hlsl_shader_model > 66) {
        result.output = "Invalid HLSL shader model " + std::to_string(hlsl_shader_model);
        result.failed = true;
        return result;
    }
    if (require_16bit_types && hlsl_shader_model < 62) {
        result.output = "The HLSL shader model " + std::to_string(hlsl_shader_model) +
                        " is not enough for float16_t.";
        result.failed = true;
        return result;
    }
    std::string shader_model_version =
        std::to_string(hlsl_shader_model / 10) + "_" + std::to_string(hlsl_shader_model % 10);

    tint::TmpFile file;
    file << source;

    for (auto ep : entry_points) {
        const char* stage_prefix = "";

        switch (ep.second) {
            case ast::PipelineStage::kNone:
                result.output = "Invalid PipelineStage";
                result.failed = true;
                return result;
            case ast::PipelineStage::kVertex:
                stage_prefix = "vs";
                break;
            case ast::PipelineStage::kFragment:
                stage_prefix = "ps";
                break;
            case ast::PipelineStage::kCompute:
                stage_prefix = "cs";
                break;
        }

        // Match Dawn's compile flags
        // See dawn\src\dawn_native\d3d12\RenderPipelineD3D12.cpp
        // and dawn_native\d3d\ShaderUtils.cpp (GetDXCArguments)
        auto res =
            dxc("-T " + std::string(stage_prefix) + "_" + shader_model_version,  // Profile
                "-HV 2018",                                                      // Use HLSL 2018
                "-E " + ep.first,                                                // Entry point
                "/Zpr",  // D3DCOMPILE_PACK_MATRIX_ROW_MAJOR
                "/Gis",  // D3DCOMPILE_IEEE_STRICTNESS
                require_16bit_types ? "-enable-16bit-types" : "",  // Enable 16-bit if required
                file.Path());
        if (!res.out.empty()) {
            if (!result.output.empty()) {
                result.output += "\n";
            }
            result.output += res.out;
        }
        if (!res.err.empty()) {
            if (!result.output.empty()) {
                result.output += "\n";
            }
            result.output += res.err;
        }
        result.failed = (res.error_code != 0);

        // Remove the temporary file name from the output to keep output deterministic
        result.output = tint::ReplaceAll(result.output, file.Path(), "shader.hlsl");
    }

    return result;
}

Result ValidateUsingDXC2(const std::string& dxc_path,
                         const std::string& source,
                         const EntryPointList& entry_points,
                         bool require_16bit_types,
                         uint32_t hlsl_shader_model) {
    Result result;

    if (entry_points.empty()) {
        result.output = "No entrypoint found";
        result.failed = true;
        return result;
    }

    // Native 16-bit types, e.g. float16_t, require SM6.2. Otherwise we use SM6.0.
    if (hlsl_shader_model < 60 || hlsl_shader_model > 66) {
        result.output = "Invalid HLSL shader model " + std::to_string(hlsl_shader_model);
        result.failed = true;
        return result;
    }
    if (require_16bit_types && hlsl_shader_model < 62) {
        result.output = "The HLSL shader model " + std::to_string(hlsl_shader_model) +
                        " is not enough for float16_t.";
        result.failed = true;
        return result;
    }
    std::wstring shader_model_version =
        std::to_wstring(hlsl_shader_model / 10) + L"_" + std::to_wstring(hlsl_shader_model % 10);

#define CHECK_HR(hr, error_msg)        \
    do {                               \
        if (FAILED(hr)) {              \
            result.output = error_msg; \
            result.failed = true;      \
            return result;             \
        }                              \
    } while (false)

    HRESULT hr;

#ifdef _WIN32
    // This library leaks if an error happens in this function, but it is ok
    // because it is loaded at most once, and the executables using it
    // are short-lived.
    HMODULE dxcLib = LoadLibraryA(dxc_path.c_str());
    if (dxcLib == nullptr) {
        result.output = "Couldn't load DXC";
        result.failed = true;
        return result;
    }
    TINT_DEFER({ FreeLibrary(dxcLib); });

    using PFN_DXC_CREATE_INSTANCE =
        HRESULT(WINAPI*)(REFCLSID rclsid, REFIID riid, _COM_Outptr_ void** ppCompiler);

    auto* dxcCreateInstance =
        reinterpret_cast<PFN_DXC_CREATE_INSTANCE>(GetProcAddress(dxcLib, "DxcCreateInstance"));
    if (dxcCreateInstance == nullptr) {
        char dll_path[_MAX_PATH] = "";
        GetModuleFileNameA(dxcLib, dll_path, sizeof(dll_path));
        result.output = "GetProcAccess failed: " + std::string(dll_path);
        result.failed = true;
        return result;
    }
    ComPtr<IDxcCompiler3> dxcCompiler;
    hr = (*dxcCreateInstance)(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    CHECK_HR(hr, "DxcCreateInstance failed");

#else
#error "TODO"
#endif

    for (auto ep : entry_points) {
        const wchar_t* stage_prefix = L"";
        switch (ep.second) {
            case ast::PipelineStage::kNone:
                result.output = "Invalid PipelineStage";
                result.failed = true;
                return result;
            case ast::PipelineStage::kVertex:
                stage_prefix = L"vs";
                break;
            case ast::PipelineStage::kFragment:
                stage_prefix = L"ps";
                break;
            case ast::PipelineStage::kCompute:
                stage_prefix = L"cs";
                break;
        }

        // Match Dawn's compile flags
        // See dawn\src\dawn_native\d3d12\RenderPipelineD3D12.cpp
        // and dawn_native\d3d\ShaderUtils.cpp (GetDXCArguments)
        std::wstring profile = std::wstring(stage_prefix) + L"_" + shader_model_version;
        std::wstring entry_point = ascii_to_wstring(ep.first);
        std::vector<const wchar_t*> args{
            L"-T",                                              // Profile
            profile.c_str(),                                    //
            L"-HV 2018",                                        // Use HLSL 2018
            L"-E",                                              // Entry point
            entry_point.c_str(),                                //
            L"/Zpr",                                            // D3DCOMPILE_PACK_MATRIX_ROW_MAJOR
            L"/Gis",                                            // D3DCOMPILE_IEEE_STRICTNESS
            require_16bit_types ? L"-enable-16bit-types" : L""  // Enable 16-bit if required
        };

        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = source.c_str();
        sourceBuffer.Size = source.length();
        sourceBuffer.Encoding = DXC_CP_UTF8;
        ComPtr<IDxcResult> compileResult;
        hr = dxcCompiler->Compile(&sourceBuffer, args.data(), args.size(), nullptr,
                                  IID_PPV_ARGS(&compileResult));
        CHECK_HR(hr, "Compile call failed");

        HRESULT compileStatus;
        hr = compileResult->GetStatus(&compileStatus);
        CHECK_HR(hr, "GetStatus call failed");

        if (FAILED(compileStatus)) {
            ComPtr<IDxcBlobEncoding> errors;
            hr = compileResult->GetErrorBuffer(&errors);
            CHECK_HR(hr, "GetErrorBuffer call failed");
            result.output = static_cast<char*>(errors->GetBufferPointer());
            result.failed = true;
            return result;
        }

        // Compilation succeeded, get compiled shader blob and disassamble it
        ComPtr<IDxcBlob> compiledShader;
        hr = compileResult->GetResult(&compiledShader);
        CHECK_HR(hr, "GetResult call failed");

        DxcBuffer blobBuffer;
        blobBuffer.Ptr = compiledShader->GetBufferPointer();
        blobBuffer.Size = compiledShader->GetBufferSize();
        blobBuffer.Encoding = DXC_CP_UTF8;
        ComPtr<IDxcResult> disResult;
        hr = dxcCompiler->Disassemble(&blobBuffer, IID_PPV_ARGS(&disResult));
        CHECK_HR(hr, "Disassemble call failed");

        ComPtr<IDxcBlobEncoding> disassembly;
        if (disResult && disResult->HasOutput(DXC_OUT_DISASSEMBLY) &&
            SUCCEEDED(
                disResult->GetOutput(DXC_OUT_DISASSEMBLY, IID_PPV_ARGS(&disassembly), nullptr))) {
            result.output = static_cast<char*>(disassembly->GetBufferPointer());
        } else {
            result.output = "Failed to disassemble shader";
        }
    }

    return result;
}

#ifdef _WIN32
Result ValidateUsingFXC(const std::string& fxc_path,
                        const std::string& source,
                        const EntryPointList& entry_points) {
    Result result;

    if (entry_points.empty()) {
        result.output = "No entrypoint found";
        result.failed = true;
        return result;
    }

    // This library leaks if an error happens in this function, but it is ok
    // because it is loaded at most once, and the executables using UsingFXC
    // are short-lived.
    HMODULE fxcLib = LoadLibraryA(fxc_path.c_str());
    if (fxcLib == nullptr) {
        result.output = "Couldn't load FXC";
        result.failed = true;
        return result;
    }

    auto* d3dCompile = reinterpret_cast<pD3DCompile>(
        reinterpret_cast<void*>(GetProcAddress(fxcLib, "D3DCompile")));
    auto* d3dDisassemble = reinterpret_cast<pD3DDisassemble>(
        reinterpret_cast<void*>(GetProcAddress(fxcLib, "D3DDisassemble")));

    if (d3dCompile == nullptr) {
        result.output = "Couldn't load D3DCompile from FXC";
        result.failed = true;
        return result;
    }
    if (d3dDisassemble == nullptr) {
        result.output = "Couldn't load D3DDisassemble from FXC";
        result.failed = true;
        return result;
    }

    for (auto ep : entry_points) {
        const char* profile = "";
        switch (ep.second) {
            case ast::PipelineStage::kNone:
                result.output = "Invalid PipelineStage";
                result.failed = true;
                return result;
            case ast::PipelineStage::kVertex:
                profile = "vs_5_1";
                break;
            case ast::PipelineStage::kFragment:
                profile = "ps_5_1";
                break;
            case ast::PipelineStage::kCompute:
                profile = "cs_5_1";
                break;
        }

        // Match Dawn's compile flags
        // See dawn\src\dawn_native\d3d12\RenderPipelineD3D12.cpp
        UINT compileFlags = D3DCOMPILE_OPTIMIZATION_LEVEL0 | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR |
                            D3DCOMPILE_IEEE_STRICTNESS;

        ComPtr<ID3DBlob> compiledShader;
        ComPtr<ID3DBlob> errors;
        HRESULT res = d3dCompile(source.c_str(),    // pSrcData
                                 source.length(),   // SrcDataSize
                                 nullptr,           // pSourceName
                                 nullptr,           // pDefines
                                 nullptr,           // pInclude
                                 ep.first.c_str(),  // pEntrypoint
                                 profile,           // pTarget
                                 compileFlags,      // Flags1
                                 0,                 // Flags2
                                 &compiledShader,   // ppCode
                                 &errors);          // ppErrorMsgs
        if (FAILED(res)) {
            result.output = static_cast<char*>(errors->GetBufferPointer());
            result.failed = true;
            return result;
        } else {
            ComPtr<ID3DBlob> disassembly;
            res = d3dDisassemble(compiledShader->GetBufferPointer(),
                                 compiledShader->GetBufferSize(), 0, "", &disassembly);
            if (FAILED(res)) {
                result.output = "Failed to disassemble shader";
            } else {
                result.output = static_cast<char*>(disassembly->GetBufferPointer());
            }
        }
    }

    FreeLibrary(fxcLib);

    return result;
}
#endif  // _WIN32

}  // namespace tint::hlsl::validate
