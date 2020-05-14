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

#include "dawn_native/d3d12/AdapterD3D12.h"

#include "common/Constants.h"
#include "dawn_native/Instance.h"
#include "dawn_native/d3d12/BackendD3D12.h"
#include "dawn_native/d3d12/DeviceD3D12.h"
#include "dawn_native/d3d12/PlatformFunctions.h"

#include <locale>

namespace dawn_native { namespace d3d12 {

    // utility wrapper to adapt locale-bound facets for wstring/wbuffer convert
    template <class Facet>
    struct DeletableFacet : Facet {
        template <class... Args>
        DeletableFacet(Args&&... args) : Facet(std::forward<Args>(args)...) {
        }

        ~DeletableFacet() {
        }
    };

    Adapter::Adapter(Backend* backend, ComPtr<IDXGIAdapter3> hardwareAdapter)
        : AdapterBase(backend->GetInstance(), wgpu::BackendType::D3D12),
          mHardwareAdapter(hardwareAdapter),
          mBackend(backend) {
    }

    const D3D12DeviceInfo& Adapter::GetDeviceInfo() const {
        return mDeviceInfo;
    }

    IDXGIAdapter3* Adapter::GetHardwareAdapter() const {
        return mHardwareAdapter.Get();
    }

    Backend* Adapter::GetBackend() const {
        return mBackend;
    }

    ComPtr<ID3D12Device> Adapter::GetDevice() const {
        return mD3d12Device;
    }

    MaybeError Adapter::Initialize() {
        // D3D12 cannot check for feature support without a device.
        // Create the device to populate the adapter properties then reuse it when needed for actual
        // rendering.
        const PlatformFunctions* functions = GetBackend()->GetFunctions();
        if (FAILED(functions->d3d12CreateDevice(GetHardwareAdapter(), D3D_FEATURE_LEVEL_11_0,
                                                _uuidof(ID3D12Device), &mD3d12Device))) {
            return DAWN_INTERNAL_ERROR("D3D12CreateDevice failed");
        }

        if (GetInstance()->IsBackendValidationEnabled()) {
            ComPtr<ID3D12InfoQueue> infoQueue;
            if (SUCCEEDED(mD3d12Device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
                D3D12_MESSAGE_ID denyIds[] = {
                    D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_HAS_NO_RESOURCE,
                    D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,

                    // TODO(enrico.galli@intel.com): Remove these after warnings have been addressed
                    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                    D3D12_MESSAGE_ID_UNMAP_RANGE_NOT_EMPTY,
                };
                D3D12_INFO_QUEUE_FILTER storageFilter = {};
                storageFilter.DenyList.NumIDs = ARRAYSIZE(denyIds);
                storageFilter.DenyList.pIDList = denyIds;
                infoQueue->PushStorageFilter(&storageFilter);

                // Retriver filter is used during strict validation. We don't error out from INFO
                // messages.
                D3D12_INFO_QUEUE_FILTER retriveFilter{};
                D3D12_MESSAGE_SEVERITY severities[] = {
                    D3D12_MESSAGE_SEVERITY_ERROR,
                    D3D12_MESSAGE_SEVERITY_WARNING,
                };
                retriveFilter.AllowList.NumSeverities = ARRAYSIZE(severities);
                retriveFilter.AllowList.pSeverityList = severities;
                infoQueue->PushRetrievalFilter(&retriveFilter);
            }
        }

        DXGI_ADAPTER_DESC1 adapterDesc;
        mHardwareAdapter->GetDesc1(&adapterDesc);

        mPCIInfo.deviceId = adapterDesc.DeviceId;
        mPCIInfo.vendorId = adapterDesc.VendorId;

        DAWN_TRY_ASSIGN(mDeviceInfo, GatherDeviceInfo(*this));

        if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            mAdapterType = wgpu::AdapterType::CPU;
        } else {
            mAdapterType = (mDeviceInfo.isUMA) ? wgpu::AdapterType::IntegratedGPU
                                               : wgpu::AdapterType::DiscreteGPU;
        }

        std::wstring_convert<DeletableFacet<std::codecvt<wchar_t, char, std::mbstate_t>>> converter(
            "Error converting");
        mPCIInfo.name = converter.to_bytes(adapterDesc.Description);

        InitializeSupportedExtensions();

        return {};
    }

    void Adapter::InitializeSupportedExtensions() {
        mSupportedExtensions.EnableExtension(Extension::TextureCompressionBC);
    }

    ResultOrError<DeviceBase*> Adapter::CreateDeviceImpl(const DeviceDescriptor* descriptor) {
        return Device::Create(this, descriptor);
    }

}}  // namespace dawn_native::d3d12
