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

#ifndef DAWNNATIVE_D3D12BACKEND_H_
#define DAWNNATIVE_D3D12BACKEND_H_

#include <dawn/dawn_wsi.h>
#include <dawn_native/DawnNative.h>

#include <DXGI1_4.h>
#include <d3d12.h>
#include <windows.h>
#include <wrl/client.h>

#include <memory>
#include <unordered_set>

struct ID3D12Device;
struct ID3D12Resource;
struct ID3D11On12Device;

namespace dawn_native { namespace d3d12 {
    DAWN_NATIVE_EXPORT Microsoft::WRL::ComPtr<ID3D12Device> GetD3D12Device(WGPUDevice device);
    DAWN_NATIVE_EXPORT DawnSwapChainImplementation CreateNativeSwapChainImpl(WGPUDevice device,
                                                                             HWND window);
    DAWN_NATIVE_EXPORT WGPUTextureFormat
    GetNativeSwapChainPreferredFormat(const DawnSwapChainImplementation* swapChain);

    enum MemorySegment {
        Local,
        NonLocal,
    };

    DAWN_NATIVE_EXPORT uint64_t SetExternalMemoryReservation(WGPUDevice device,
                                                             uint64_t requestedReservationSize,
                                                             MemorySegment memorySegment);

    struct DAWN_NATIVE_EXPORT ExternalImageDescriptorDXGISharedHandle : ExternalImageDescriptor {
      public:
        ExternalImageDescriptorDXGISharedHandle();

        // Note: SharedHandle must be a handle to a texture object.
        HANDLE sharedHandle;
    };

    struct DAWN_NATIVE_EXPORT ExternalImageAccessDescriptorDXGIKeyedMutex
        : ExternalImageAccessDescriptor {
      public:
        uint64_t acquireMutexKey;
        // Release key will be set to acquireMutexKey + 1 if set to sentinel value UINT64_MAX.
        uint64_t releaseMutexKey = UINT64_MAX;
        bool isSwapChainTexture = false;
    };

    class DAWN_NATIVE_EXPORT D3D11on12Resource {
      public:
        D3D11on12Resource(Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11on12Device);
        D3D11on12Resource(Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dxgiKeyedMutex,
                          Microsoft::WRL::ComPtr<ID3D11On12Device> d3d11on12Device);
        ~D3D11on12Resource();

        HRESULT BeginAccess(uint64_t acquireMutexKey);
        void EndAccess(uint64_t releaseMutexKey);

        // Functors necessary for the
        // unordered_set<std::shared_ptr<D3D11on12Resource>>-based cache.
        struct HashFunc {
            size_t operator()(const std::shared_ptr<D3D11on12Resource>& a) const;
        };

        struct EqualityFunc {
            bool operator()(const std::shared_ptr<D3D11on12Resource>& a,
                            const std::shared_ptr<D3D11on12Resource>& b) const;
        };

      private:
        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> mDXGIKeyedMutex;
        Microsoft::WRL::ComPtr<ID3D11On12Device> mD3D11on12Device;
    };

    class DAWN_NATIVE_EXPORT ExternalImageDXGI {
      public:
        // Note: SharedHandle must be a handle to a texture object.
        static std::unique_ptr<ExternalImageDXGI> Create(
            WGPUDevice device,
            const ExternalImageDescriptorDXGISharedHandle* descriptor);

        WGPUTexture ProduceTexture(WGPUDevice device,
                                   const ExternalImageAccessDescriptorDXGIKeyedMutex* descriptor);

      private:
        ExternalImageDXGI(Microsoft::WRL::ComPtr<ID3D12Resource> d3d12Resource,
                          const WGPUTextureDescriptor* descriptor);

        std::shared_ptr<D3D11on12Resource> GetOrCreateD3D11on12Resource(WGPUDevice device);

        Microsoft::WRL::ComPtr<ID3D12Resource> mD3D12Resource;

        // Contents of WGPUTextureDescriptor are stored individually since the descriptor
        // could outlive this image.
        WGPUTextureUsageFlags mUsage;
        WGPUTextureDimension mDimension;
        WGPUExtent3D mSize;
        WGPUTextureFormat mFormat;
        uint32_t mMipLevelCount;
        uint32_t mSampleCount;

        static constexpr uint64_t kMaxResourceCacheSize = 5;
        using D3D11on12ResourceCache = std::unordered_set<std::shared_ptr<D3D11on12Resource>,
                                                          D3D11on12Resource::HashFunc,
                                                          D3D11on12Resource::EqualityFunc>;

        D3D11on12ResourceCache mD3D11on12Resources;
    };

    struct DAWN_NATIVE_EXPORT AdapterDiscoveryOptions : public AdapterDiscoveryOptionsBase {
        AdapterDiscoveryOptions(Microsoft::WRL::ComPtr<IDXGIAdapter> adapter);

        Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12BACKEND_H_
