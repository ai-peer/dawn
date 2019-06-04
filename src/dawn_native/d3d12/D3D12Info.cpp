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

#include "dawn_native/d3d12/D3D12Info.h"

#include "dawn_native/D3D12/AdapterD3D12.h"
#include "dawn_native/D3D12/BackendD3D12.h"

namespace dawn_native { namespace d3d12 {

    ResultOrError<D3D12DeviceInfo> GatherDeviceInfo(const Adapter& adapter) {
        const PlatformFunctions* functions = adapter.GetBackend()->GetFunctions();

        ComPtr<ID3D12Device> device;
        if (FAILED(functions->d3d12CreateDevice(adapter.GetHardwareAdapter(),
                                                D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device),
                                                &device))) {
            return DAWN_CONTEXT_LOST_ERROR("D3D12CreateDevice failed");
        }

        D3D12DeviceInfo info = {};

        // Gather info about device memory
        {
            // Query for the adapter's architectural details.
            // Note: D3D_FEATURE_DATA_ARCHITECTURE1 is only enabled on newer Win10 builds
            // https://docs.microsoft.com/en-us/windows/desktop/api/d3d12/ne-d3d12-d3d12_feature
            D3D12_FEATURE_DATA_ARCHITECTURE1 arch1 = {};
            if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1, &arch1,
                                                   sizeof(arch1)))) {
                return DAWN_CONTEXT_LOST_ERROR("CheckFeatureSupport failed");
            }

            info.UMA = arch1.UMA;
        }

        return info;
    }
}}  // namespace dawn_native::d3d12