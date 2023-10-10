// Copyright 2023 The Dawn Authors
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

#include "dawn/native/d3d/DeviceD3D.h"

#include "dawn/native/d3d/BackendD3D.h"
#include "dawn/native/d3d/ExternalImageDXGIImpl.h"
#include "dawn/native/d3d/Forward.h"
#include "dawn/native/d3d/PhysicalDeviceD3D.h"

namespace dawn::native::d3d {

Device::Device(AdapterBase* adapter,
               const DeviceDescriptor* descriptor,
               const TogglesState& deviceToggles)
    : DeviceBase(adapter, descriptor, deviceToggles) {}

Device::~Device() {
    Destroy();

    // Close the handle here instead of in DestroyImpl. The handle is returned from
    // ExternalImageDXGI, so it needs to live as long as the Device ref does, even if the device
    // state is destroyed.
    if (mFenceHandle != nullptr) {
        ::CloseHandle(mFenceHandle);
        mFenceHandle = nullptr;
    }
}

void Device::DestroyImpl() {
    while (!mExternalImageList.empty()) {
        d3d::ExternalImageDXGIImpl* externalImage = mExternalImageList.head()->value();
        // ExternalImageDXGIImpl::DestroyInternal() calls RemoveFromList().
        externalImage->DestroyInternal();
    }
}

ResultOrError<wgpu::TextureUsage> Device::GetSupportedSurfaceUsageImpl(
    const Surface* surface) const {
    wgpu::TextureUsage usages = wgpu::TextureUsage::RenderAttachment |
                                wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc |
                                wgpu::TextureUsage::CopyDst;
    return usages;
}

const PlatformFunctions* Device::GetFunctions() const {
    return ToBackend(GetPhysicalDevice())->GetBackend()->GetFunctions();
}

ComPtr<IDXGIFactory4> Device::GetFactory() const {
    return ToBackend(GetPhysicalDevice())->GetBackend()->GetFactory();
}

HANDLE Device::GetFenceHandle() const {
    return mFenceHandle;
}

std::unique_ptr<ExternalImageDXGIImpl> Device::CreateExternalImageDXGIImpl(
    const ExternalImageDescriptor* descriptor) {
    std::unique_ptr<ExternalImageDXGIImpl> externalImage;
    if (!ConsumedError(CreateExternalImageDXGIImplImpl(descriptor), &externalImage)) {
        mExternalImageList.Append(externalImage.get());
        return externalImage;
    }
    return {};
}

}  // namespace dawn::native::d3d
