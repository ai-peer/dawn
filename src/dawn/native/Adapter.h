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

#ifndef SRC_DAWN_NATIVE_ADAPTER_H_
#define SRC_DAWN_NATIVE_ADAPTER_H_

#include <string>
#include <vector>

#include "dawn/native/DawnNative.h"

#include "dawn/common/GPUInfo.h"
#include "dawn/common/RefCounted.h"
#include "dawn/common/ityp_span.h"
#include "dawn/native/Error.h"
#include "dawn/native/Features.h"
#include "dawn/native/Limits.h"
#include "dawn/native/Toggles.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

class DeviceBase;

// TODO: Decide if keep this.
/*
class AdapterTogglesState {
  public:
    AdapterTogglesState() : mTogglesState(ToggleInfo::ToggleStage::Adapter) {}
    AdapterTogglesState(TogglesState togglesState) : mTogglesState(togglesState) {
        ASSERT(togglesState.togglesStateStage == ToggleInfo::ToggleStage::Adapter);
    }

    AdapterTogglesState& operator=(const TogglesState&) = delete;
    AdapterTogglesState& operator=(TogglesState&&) = delete;

    void SetTogglesState(const TogglesState& togglesState) {
        ASSERT(togglesState.togglesStateStage == ToggleInfo::ToggleStage::Adapter);
        mTogglesState = togglesState;
    }

    const TogglesState& GetTogglesState() const { return mTogglesState; }

    bool IsEnabled(Toggle toggle) const { return mTogglesState.IsEnabled(toggle); }
    bool IsDisabled(Toggle toggle) const { return mTogglesState.IsDisabled(toggle); }
    bool IsForced(Toggle toggle) const { return mTogglesState.IsForced(toggle); }
    bool IsSet(Toggle toggle) const { return mTogglesState.IsSet(toggle); }
    std::vector<const char*> GetEnabledToggleNames() const {
        return mTogglesState.GetEnabledToggleNames();
    }
    std::vector<const char*> GetDisabledToggleNames() const {
        return mTogglesState.GetDisabledToggleNames();
    }

  private:
    TogglesState mTogglesState;
};
*/
using AdapterTogglesState = TogglesState;

class AdapterBase : public RefCounted {
  public:
    AdapterBase(InstanceBase* instance,
                wgpu::BackendType backend,
                const TogglesState& adapterToggles);
    ~AdapterBase() override;

    MaybeError Initialize();

    // WebGPU API
    bool APIGetLimits(SupportedLimits* limits) const;
    void APIGetProperties(AdapterProperties* properties) const;
    bool APIHasFeature(wgpu::FeatureName feature) const;
    size_t APIEnumerateFeatures(wgpu::FeatureName* features) const;
    void APIRequestDevice(const DeviceDescriptor* descriptor,
                          WGPURequestDeviceCallback callback,
                          void* userdata);
    DeviceBase* APICreateDevice(const DeviceDescriptor* descriptor = nullptr);

    uint32_t GetVendorId() const;
    uint32_t GetDeviceId() const;
    const gpu_info::DriverVersion& GetDriverVersion() const;
    wgpu::BackendType GetBackendType() const;
    InstanceBase* GetInstance() const;

    void ResetInternalDeviceForTesting();

    FeaturesSet GetSupportedFeatures() const;
    bool SupportsAllRequiredFeatures(
        const ityp::span<size_t, const wgpu::FeatureName>& features) const;
    FeaturesSet GetSupportedFeaturesUnderToggles(const AdapterTogglesState& toggles) const;

    bool GetLimits(SupportedLimits* limits) const;

    void SetUseTieredLimits(bool useTieredLimits);

    // Instead of storing required toggles in adapter, just check adapter toggles state with
    // backend::MakeAdapterToggles.
    /*
    // Get the adapter toggles set required when creating adapters. This required toggles set does
    // not necessary match the actual toggles state of the adapter, as some toggles may be forced
    // enabled or disabled.
    const RequiredTogglesSet& GetRequiredAdapterTogglesSet() const;
    */

    // Get the actual toggles state of the adapter.
    const TogglesState& GetAdapterTogglesState() const;
    void SetAdapterTogglesForTesting(const TogglesState& adapterToggles);

    virtual bool SupportsExternalImages() const = 0;

  protected:
    uint32_t mVendorId = 0xFFFFFFFF;
    std::string mVendorName;
    std::string mArchitectureName;
    uint32_t mDeviceId = 0xFFFFFFFF;
    std::string mName;
    wgpu::AdapterType mAdapterType = wgpu::AdapterType::Unknown;
    gpu_info::DriverVersion mDriverVersion;
    std::string mDriverDescription;

    // Add a supported feature into mSupportedFeatures. If the given feature is of
    // FeatureState::Experimental, the feature will be added if and only if adapter has toggle
    // DisallowUnsafeAPIs disabled.
    void EnableFeature(FeaturesSet& featuresSet, Feature feature) const;
    // Used for the tests that intend to use an adapter without all features enabled.
    void SetSupportedFeaturesForTesting(const std::vector<wgpu::FeatureName>& requiredFeatures);

  private:
    TogglesState MakeDeviceToggles(const RequiredTogglesSet& requiredDeviceToggles) const;
    virtual TogglesState MakeDeviceTogglesImpl(
        const RequiredTogglesSet& requiredDeviceToggles) const = 0;

    virtual ResultOrError<Ref<DeviceBase>> CreateDeviceImpl(const DeviceDescriptor* descriptor,
                                                            const TogglesState& deviceToggles) = 0;

    virtual MaybeError InitializeImpl() = 0;

    // Check base WebGPU features and discover supported features.
    virtual FeaturesSet GetSupportedFeaturesUnderTogglesImpl(
        const AdapterTogglesState& toggles) const = 0;

    // Check base WebGPU limits and populate supported limits.
    virtual MaybeError InitializeSupportedLimitsImpl(CombinedLimits* limits) = 0;

    virtual void InitializeVendorArchitectureImpl();

    ResultOrError<Ref<DeviceBase>> CreateDeviceInternal(const DeviceDescriptor* descriptor);

    virtual MaybeError ResetInternalDeviceForTestingImpl();
    Ref<InstanceBase> mInstance;
    wgpu::BackendType mBackend;
    // TODO: Change to AdapterTogglesState
    // TogglesState mAdapterTogglesState;
    AdapterTogglesState mAdapterTogglesState;
    // RequiredTogglesSet mAdapterRequiredTogglesSet;

    // Features set that CAN be supported by devices of this adapter. Some features in this set may
    // be guarded by toggles, and creating a device with these features required may result in a
    // validation error if proper toggles are not enabled/disabled.
    FeaturesSet mSupportedFeatures;

    CombinedLimits mLimits;
    bool mUseTieredLimits = false;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_ADAPTER_H_
