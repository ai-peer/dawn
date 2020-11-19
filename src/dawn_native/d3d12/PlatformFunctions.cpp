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
#include <array>
#include <sstream>

namespace dawn_native {

    namespace {
        // Extract Version from "10.0.{Version}.0" if possible, otherwise return 0.
        uint32_t GetWindowsSDKVersionFromDirectoryName(const char* directoryName) {
            const char* kPrefix = "10.0.";
            const char* kPostfix = ".0";

            if (strlen(directoryName) < strlen(kPrefix) + strlen(kPostfix) + 1) {
                return 0;
            }

            // Check if directoryName starts with "10.0.".
            if (strncmp(directoryName, kPrefix, strlen(kPrefix)) != 0) {
                return 0;
            }

            // Check if directoryName ends with ".0".
            if (strncmp(directoryName + (strlen(directoryName) - strlen(kPostfix)), kPostfix,
                        strlen(kPostfix)) != 0) {
                return 0;
            }

            // Extract Version from "10.0.{Version}.0" and convert Version into an integer.
            std::vector<char> version(strlen(directoryName) - strlen(kPrefix) - strlen(kPostfix) +
                                      1);
            for (size_t i = 0; i < version.size() - 1; ++i) {
                char ch = directoryName[strlen(kPrefix) + i];
                if (!isdigit(ch)) {
                    return 0;
                }
                version[i] = ch;
            }
            version[version.size() - 1] = '\0';
            return atoi(version.data());
        }

        std::string GetWindowsSDKBasePath() {
            const char* kDefaultWindowsSDKPath =
                "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\*";
            WIN32_FIND_DATAA fileData;
            HANDLE handle = FindFirstFileA(kDefaultWindowsSDKPath, &fileData);
            if (handle == INVALID_HANDLE_VALUE) {
                return "";
            }

            uint32_t highestWindowsSDKVersion = 0;
            do {
                if (!(fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    continue;
                }

                highestWindowsSDKVersion =
                    std::max(highestWindowsSDKVersion,
                             GetWindowsSDKVersionFromDirectoryName(fileData.cFileName));
            } while (FindNextFileA(handle, &fileData));

            if (highestWindowsSDKVersion == 0) {
                return "";
            }

            // Currently we only support using DXC on x64.
            std::ostringstream ostream;
            ostream << "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0."
                    << highestWindowsSDKVersion << ".0\\x64\\";

            return ostream.str();
        }
    }  // anonymous namespace

    namespace d3d12 {

        PlatformFunctions::PlatformFunctions() {
        }
        PlatformFunctions::~PlatformFunctions() {
        }

        MaybeError PlatformFunctions::LoadFunctions() {
            DAWN_TRY(LoadD3D12());
            DAWN_TRY(LoadDXGI());
            LoadDXCLibraries();
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

        void PlatformFunctions::LoadDXCLibraries() {
            std::string windowsSDKBasePath = GetWindowsSDKBasePath();

            LoadDXIL(windowsSDKBasePath);
            LoadDXCompiler(windowsSDKBasePath);
        }

        void PlatformFunctions::LoadDXIL(const std::string& baseWindowsSDKPath) {
            const char* dxilDLLName = "dxil.dll";
            const std::array<std::string, 2> kDxilDLLPaths = {
                {dxilDLLName, baseWindowsSDKPath + dxilDLLName}};

            for (const std::string& dxilDLLPath : kDxilDLLPaths) {
                if (mDXILLib.Open(dxilDLLPath, nullptr)) {
                    return;
                }
            }
            mDXILLib.Close();
        }

        bool PlatformFunctions::OpenDXCompiler(const std::string& baseWindowsSDKPath) {
            const char* dxCompilerDLLName = "dxcompiler.dll";
            const std::array<std::string, 2> kDxCompilerDLLPaths = {
                {dxCompilerDLLName, baseWindowsSDKPath + dxCompilerDLLName}};

            for (const std::string& dxCompilerDLLName : kDxCompilerDLLPaths) {
                if (mDXCompilerLib.Open(dxCompilerDLLName, nullptr)) {
                    return true;
                }
            }

            mDXCompilerLib.Close();
            return false;
        }

        void PlatformFunctions::LoadDXCompiler(const std::string& baseWindowsSDKPath) {
            // DXIL must be loaded before DXC, otherwise shader signing is unavailable
            if (!mDXILLib.Valid()) {
                return;
            }

            if (!OpenDXCompiler(baseWindowsSDKPath) ||
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
                !mPIXEventRuntimeLib.GetProc(&pixEndEventOnCommandList,
                                             "PIXEndEventOnCommandList") ||
                !mPIXEventRuntimeLib.GetProc(&pixSetMarkerOnCommandList,
                                             "PIXSetMarkerOnCommandList")) {
                mPIXEventRuntimeLib.Close();
            }
        }

    }  // namespace d3d12
}  // namespace dawn_native
