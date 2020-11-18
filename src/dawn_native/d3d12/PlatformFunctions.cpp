// Copyright 2018 The Dawn Authors
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

#include "dawn_native/d3d12/PlatformFunctions.h"

#include "common/DynamicLib.h"

#include <comdef.h>
#include <regex>
#include <sstream>

namespace dawn_native { namespace d3d12 {

    PlatformFunctions::PlatformFunctions() {
    }
    PlatformFunctions::~PlatformFunctions() {
    }

    MaybeError PlatformFunctions::LoadFunctions() {
        DAWN_TRY(LoadD3D12());
        DAWN_TRY(LoadDXGI());
        GetWindowsSDKBasePath();
        LoadDXIL();
        LoadDXCompiler();
        DAWN_TRY(LoadFXCompiler());
        DAWN_TRY(LoadD3D11());
        LoadPIXRuntime();
        return {};
    }

    MaybeError PlatformFunctions::LoadD3D12() {
        std::string error;
        if (!mD3D12Lib.Open("d3d12.dll", &error) ||
            !mD3D12Lib.GetProc(&d3d12CreateDevice, "D3D12CreateDevice", &error) ||
            !mD3D12Lib.GetProc(&d3d12GetDebugInterface, "D3D12GetDebugInterface", &error) ||
            !mD3D12Lib.GetProc(&d3d12SerializeRootSignature, "D3D12SerializeRootSignature",
                               &error) ||
            !mD3D12Lib.GetProc(&d3d12CreateRootSignatureDeserializer,
                               "D3D12CreateRootSignatureDeserializer", &error) ||
            !mD3D12Lib.GetProc(&d3d12SerializeVersionedRootSignature,
                               "D3D12SerializeVersionedRootSignature", &error) ||
            !mD3D12Lib.GetProc(&d3d12CreateVersionedRootSignatureDeserializer,
                               "D3D12CreateVersionedRootSignatureDeserializer", &error)) {
            return DAWN_INTERNAL_ERROR(error.c_str());
        }

        return {};
    }

    MaybeError PlatformFunctions::LoadD3D11() {
        std::string error;
        if (!mD3D11Lib.Open("d3d11.dll", &error) ||
            !mD3D11Lib.GetProc(&d3d11on12CreateDevice, "D3D11On12CreateDevice", &error)) {
            return DAWN_INTERNAL_ERROR(error.c_str());
        }

        return {};
    }

    MaybeError PlatformFunctions::LoadDXGI() {
        std::string error;
        if (!mDXGILib.Open("dxgi.dll", &error) ||
            !mDXGILib.GetProc(&dxgiGetDebugInterface1, "DXGIGetDebugInterface1", &error) ||
            !mDXGILib.GetProc(&createDxgiFactory2, "CreateDXGIFactory2", &error)) {
            return DAWN_INTERNAL_ERROR(error.c_str());
        }

        return {};
    }

    void PlatformFunctions::GetWindowsSDKBasePath() {
        const char* kDefaultWindowsSDKPath = "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\*";
        WIN32_FIND_DATAA fileData;
        HANDLE handle = FindFirstFileA(kDefaultWindowsSDKPath, &fileData);
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }

        std::smatch match;
        std::regex windowsSDKVersionRegex("10\\.0\\.([0-9]+)\\.0");

        uint32_t highestWindowsSDKVersion = 0;
        do {
            if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                continue;
            }

            const std::string pathStr = fileData.cFileName;
            if (std::regex_search(pathStr, match, windowsSDKVersionRegex)) {
                uint32_t windowsSDKVersion = std::atoi(match[1].str().c_str());
                highestWindowsSDKVersion = std::max(highestWindowsSDKVersion, windowsSDKVersion);
            }
        } while (FindNextFileA(handle, &fileData));

        if (highestWindowsSDKVersion == 0) {
            return;
        }

        // Currently we only support using DXC on x64.
        std::ostringstream ostream;
        ostream << "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0."
                << highestWindowsSDKVersion << ".0\\x64\\";

        mWindowsSDKBasePath = ostream.str();
    }

    void PlatformFunctions::LoadDXIL() {
        const char* dxilDLLName = "dxil.dll";

        if (mDXILLib.Open(dxilDLLName, nullptr)) {
            return;
        }

        if (mWindowsSDKBasePath.empty()) {
            mDXILLib.Close();
            return;
        }

        if (!mDXILLib.Open(mWindowsSDKBasePath + dxilDLLName, nullptr)) {
            mDXILLib.Close();
        }
    }

    bool PlatformFunctions::OpenDXCompiler() {
        const char* dxcompilerDLLName = "dxcompiler.dll";

        if (mDXCompilerLib.Open(dxcompilerDLLName, nullptr)) {
            return true;
        }

        if (mWindowsSDKBasePath.empty()) {
            return false;
        }

        return mDXCompilerLib.Open(mWindowsSDKBasePath + dxcompilerDLLName, nullptr);
    }

    void PlatformFunctions::LoadDXCompiler() {
        // DXIL must be loaded before DXC, otherwise shader signing is unavailable
        if (!OpenDXCompiler() ||
            !mDXCompilerLib.GetProc(&dxcCreateInstance, "DxcCreateInstance", nullptr)) {
            mDXCompilerLib.Close();
        }
    }

    MaybeError PlatformFunctions::LoadFXCompiler() {
        std::string error;
        if (!mFXCompilerLib.Open("d3dcompiler_47.dll", &error) ||
            !mFXCompilerLib.GetProc(&d3dCompile, "D3DCompile", &error)) {
            return DAWN_INTERNAL_ERROR(error.c_str());
        }

        return {};
    }

    bool PlatformFunctions::IsPIXEventRuntimeLoaded() const {
        return mPIXEventRuntimeLib.Valid();
    }

    bool PlatformFunctions::IsDXCAvailable() const {
        return mDXILLib.Valid() && mDXCompilerLib.Valid();
    }

    void PlatformFunctions::LoadPIXRuntime() {
        if (!mPIXEventRuntimeLib.Open("WinPixEventRuntime.dll") ||
            !mPIXEventRuntimeLib.GetProc(&pixBeginEventOnCommandList,
                                         "PIXBeginEventOnCommandList") ||
            !mPIXEventRuntimeLib.GetProc(&pixEndEventOnCommandList, "PIXEndEventOnCommandList") ||
            !mPIXEventRuntimeLib.GetProc(&pixSetMarkerOnCommandList, "PIXSetMarkerOnCommandList")) {
            mPIXEventRuntimeLib.Close();
        }
    }

}}  // namespace dawn_native::d3d12
