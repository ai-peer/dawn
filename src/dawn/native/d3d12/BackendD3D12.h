// Copyright 2019 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_D3D12_BACKENDD3D12_H_
#define SRC_DAWN_NATIVE_D3D12_BACKENDD3D12_H_

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "dawn/native/BackendConnection.h"

#include "dawn/native/d3d12/d3d12_platform.h"

namespace dawn::native::d3d12 {

class PlatformFunctions;

// DxcVersionInfo holds both DXC compiler (dxcompiler.dll) version and DXC validator (dxil.dll)
// version, which are not necessarily identical. Both are in uint64_t type, as the result of
// MakeDXCVersion.
struct DxcVersionInfo {
    uint64_t DxcCompilerVersion;
    uint64_t DxcValidatorVersion;
};

class Backend : public BackendConnection {
  public:
    explicit Backend(InstanceBase* instance);

    MaybeError Initialize();

    ComPtr<IDXGIFactory4> GetFactory() const;

    MaybeError EnsureDxcLibrary();
    MaybeError EnsureDxcCompiler();
    MaybeError EnsureDxcValidator();
    ComPtr<IDxcLibrary> GetDxcLibrary() const;
    ComPtr<IDxcCompiler> GetDxcCompiler() const;
    ComPtr<IDxcValidator> GetDxcValidator() const;

    // Return std::nullopt if Dxc is not available.
    std::optional<DxcVersionInfo> GetDxcVersion();

    // Return true if and only if DXC binary is avaliable, and the DXC compiler and validator
    // version are validated to be no older than a specific minimium version, currently 1.6.
    bool IsDXCAvailable();

    // Return true if and only if IsDXCAvailable() return true, and the DXC compiler and validator
    // version are validated to be no older than the minimium version given in parameter.
    bool IsDXCAvailableAndVersionAtLeast(uint64_t minimumCompilerMajorVersion,
                                         uint64_t minimumCompilerMinorVersion,
                                         uint64_t minimumValidatorMajorVersion,
                                         uint64_t minimumValidatorMinorVersion);

    const PlatformFunctions* GetFunctions() const;

    std::vector<Ref<AdapterBase>> DiscoverDefaultAdapters() override;
    ResultOrError<std::vector<Ref<AdapterBase>>> DiscoverAdapters(
        const AdapterDiscoveryOptionsBase* optionsBase) override;

  private:
    // Keep mFunctions as the first member so that in the destructor it is freed last. Otherwise
    // the D3D12 DLLs are unloaded before we are done using them.
    std::unique_ptr<PlatformFunctions> mFunctions;
    ComPtr<IDXGIFactory4> mFactory;
    ComPtr<IDxcLibrary> mDxcLibrary;
    ComPtr<IDxcCompiler> mDxcCompiler;
    ComPtr<IDxcValidator> mDxcValidator;

    // Cache of DXC version information. There are three possible states:
    //   1. The DXC version information have not been checked yet, represented by std::monostate;
    //   2. The DXC binary is not available or error occures when checking the version information,
    //      and therefore no DXC version information available, represented by empty std::optional;
    //   3. The DXC version information is acquired successfully, stored in std::optional.
    // This cache is updated by GetDxcVersion.
    std::variant<std::monostate, std::optional<DxcVersionInfo>> mDxcVersionInfo;
};

}  // namespace dawn::native::d3d12

#endif  // SRC_DAWN_NATIVE_D3D12_BACKENDD3D12_H_
