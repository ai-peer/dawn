// Copyright 2019 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_NATIVE_METAL_BACKENDMTL_H_
#define SRC_DAWN_NATIVE_METAL_BACKENDMTL_H_

#include <vector>

#include "dawn/common/NSRef.h"
#include "dawn/native/BackendConnection.h"
#include "dawn/native/PhysicalDevice.h"

#import <Metal/Metal.h>

namespace dawn::native::metal {

class Backend : public BackendConnection {
  public:
    explicit Backend(InstanceBase* instance);
    ~Backend() override;

    std::vector<Ref<PhysicalDeviceBase>> DiscoverPhysicalDevices(
        const RequestAdapterOptions* options) override;
    void ClearPhysicalDevices() override;
    size_t GetPhysicalDeviceCountForTesting() const override;

  private:
    std::vector<Ref<PhysicalDeviceBase>> mPhysicalDevices;
};

// TODO(dawn:2155): move this PhysicalDevice class to PhysicalDeviceMTL.h
class PhysicalDevice : public PhysicalDeviceBase {
  public:
    PhysicalDevice(InstanceBase* instance,
                   NSPRef<id<MTLDevice>> device,
                   bool metalValidationEnabled);

    // PhysicalDeviceBase Implementation
    bool SupportsExternalImages() const override;

    bool SupportsFeatureLevel(FeatureLevel) const override;

    bool IsMetalValidationLayerEnabled() const { return mMetalValidationLayerEnabled; }

  private:
    ResultOrError<Ref<DeviceBase>> CreateDeviceImpl(AdapterBase* adapter,
                                                    const DeviceDescriptor* descriptor,
                                                    const TogglesState& deviceToggles) override;

    void SetupBackendAdapterToggles(TogglesState* adapterToggles) const override;

    void SetupBackendDeviceToggles(TogglesState* deviceToggles) const override;

    MaybeError InitializeImpl() override;

    void InitializeSupportedFeaturesImpl() override;

    void InitializeVendorArchitectureImpl() override;

    enum class MTLGPUFamily {
        Apple1,
        Apple2,
        Apple3,
        Apple4,
        Apple5,
        Apple6,
        Apple7,
        Mac1,
        Mac2,
    };

    ResultOrError<MTLGPUFamily> GetMTLGPUFamily() const;

    MaybeError InitializeSupportedLimitsImpl(CombinedLimits* limits) override;

    MaybeError ValidateFeatureSupportedWithTogglesImpl(wgpu::FeatureName feature,
                                                       const TogglesState& toggles) const override;

    NSPRef<id<MTLDevice>> mDevice;
    const bool mMetalValidationLayerEnabled;
};

}  // namespace dawn::native::metal

#endif  // SRC_DAWN_NATIVE_METAL_BACKENDMTL_H_
