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

#ifndef SRC_DAWN_NATIVE_D3D_SHARED_TEXTURE_MEMORY_D3D_H_
#define SRC_DAWN_NATIVE_D3D_SHARED_TEXTURE_MEMORY_D3D_H_

#include <utility>

#include "dawn/native/SharedTextureMemory.h"
#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d/DeviceD3D.h"
#include "dawn/native/d3d/Forward.h"
#include "dawn/native/d3d/SharedFenceD3D.h"
#include "dawn/native/d3d/UtilsD3D.h"
#include "dawn/native/d3d11/Forward.h"
#include "dawn/native/d3d12/Forward.h"

namespace dawn::native::d3d {

template <typename BackendTraits, typename ID3DResourceType>
class SharedTextureMemory : public SharedTextureMemoryBase {
  public:
    ID3DResourceType* GetD3DResource() const { return mD3DResource.Get(); }

  protected:
    SharedTextureMemory(typename BackendTraits::DeviceType* device,
                        const char* label,
                        SharedTextureMemoryProperties properties,
                        ComPtr<ID3DResourceType> d3dResource)
        : SharedTextureMemoryBase(device, label, properties), mD3DResource(d3dResource) {
        // If the resource has IDXGIKeyedMutex interface, it will be used for synchronization.
        // TODO(dawn:1906): remove the mDXGIKeyedMutex when it is not used in chrome.
        mD3DResource.As(&mDXGIKeyedMutex);
    }

  protected:
    void DestroyImpl() override { mD3DResource = nullptr; }

    ResultOrError<Ref<TextureBase>> CreateTextureImpl(
        const TextureDescriptor* descriptor) override {
        return BackendTraits::TextureType::CreateFromSharedTextureMemory(
            static_cast<BackendTraits::SharedTextureMemoryType*>(this), descriptor);
    }

    MaybeError BeginAccessImpl(TextureBase* texture,
                               const BeginAccessDescriptor* descriptor) override {
        for (size_t i = 0; i < descriptor->fenceCount; ++i) {
            SharedFenceBase* fence = descriptor->fences[i];

            SharedFenceExportInfo exportInfo;
            fence->APIExportInfo(&exportInfo);
            switch (exportInfo.type) {
                case wgpu::SharedFenceType::DXGISharedHandle:
                    DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceDXGISharedHandle),
                                    "Required feature (%s) for %s is missing.",
                                    wgpu::FeatureName::SharedFenceDXGISharedHandle,
                                    wgpu::SharedFenceType::DXGISharedHandle);
                    break;
                default:
                    return DAWN_VALIDATION_ERROR("Unsupported fence type %s.", exportInfo.type);
            }
        }

        if (mDXGIKeyedMutex) {
            DAWN_TRY(CheckHRESULT(mDXGIKeyedMutex->AcquireSync(kDXGIKeyedMutexAcquireKey, INFINITE),
                                  "Acquire keyed mutex"));
        }
        return {};
    }

    ResultOrError<FenceAndSignalValue> EndAccessImpl(TextureBase* texture) override {
        DAWN_INVALID_IF(!GetDevice()->HasFeature(Feature::SharedFenceDXGISharedHandle),
                        "Required feature (%s) is missing.",
                        wgpu::FeatureName::SharedFenceDXGISharedHandle);

        if (mDXGIKeyedMutex) {
            mDXGIKeyedMutex->ReleaseSync(kDXGIKeyedMutexAcquireKey);
        }

        SharedFenceDXGISharedHandleDescriptor desc;
        desc.handle = ToBackend(GetDevice())->GetFenceHandle();

        Ref<typename BackendTraits::SharedFenceType> fence;
        DAWN_TRY_ASSIGN(fence, BackendTraits::SharedFenceType::Create(
                                   ToBackend(GetDevice()), "Internal shared DXGI fence", &desc));

        return FenceAndSignalValue{std::move(fence), static_cast<uint64_t>(GetLastUsageSerial())};
    }

  private:
    ComPtr<ID3DResourceType> mD3DResource;

    // If the resource has IDXGIKeyedMutex interface, it will be used for synchronization.
    // TODO(dawn:1906): remove the mDXGIKeyedMutex when it is not used in chrome.
    ComPtr<IDXGIKeyedMutex> mDXGIKeyedMutex;
    // Chrome uses 0 as acquire key.
    static constexpr UINT64 kDXGIKeyedMutexAcquireKey = 0;
};

}  // namespace dawn::native::d3d

#if DAWN_COMPILER_IS(MSVC)

// Include the backend derived types on MSVC which wrongly tries to instantiate
// the extern template.

#if defined(DAWN_ENABLE_BACKEND_D3D11)
#include "dawn/native/d3d11/SharedFenceD3D11.h"
#include "dawn/native/d3d11/TextureD3D11.h"
#endif

#if defined(DAWN_ENABLE_BACKEND_D3D12)
#include "dawn/native/d3d12/SharedFenceD3D12.h"
#include "dawn/native/d3d12/TextureD3D12.h"
#endif

#else  // DAWN_COMPILER_IS(MSVC)

// With other compilers, declare derived templates as extern. They are instantiated explictly
// in the dervied implementation's .cpp file.

namespace dawn::native {

extern template class d3d::SharedTextureMemory<d3d11::D3D11BackendTraits, ID3D11Resource>;
extern template class d3d::SharedTextureMemory<d3d12::D3D12BackendTraits, ID3D12Resource>;

}  // namespace dawn::native

#endif  // DAWN_COMPILER_IS(MSVC)

#endif  // SRC_DAWN_NATIVE_D3D_SHARED_TEXTURE_MEMORY_D3D_H_
