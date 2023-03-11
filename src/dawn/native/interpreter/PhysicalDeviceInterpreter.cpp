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

#include "dawn/native/interpreter/PhysicalDeviceInterpreter.h"

#include "dawn/native/ErrorData.h"
#include "dawn/native/Instance.h"
#include "dawn/native/interpreter/DeviceInterpreter.h"

namespace dawn::native::interpreter {

PhysicalDevice::PhysicalDevice(InstanceBase* instance)
    : PhysicalDeviceBase(instance, wgpu::BackendType::WgslInterpreter) {
    mVendorId = 0;
    mDeviceId = 0;
    mName = "WGSL interpreter backend";
    mAdapterType = wgpu::AdapterType::CPU;
    MaybeError err = Initialize();
    DAWN_ASSERT(err.IsSuccess());
}

PhysicalDevice::~PhysicalDevice() = default;

bool PhysicalDevice::SupportsExternalImages() const {
    return false;
}

bool PhysicalDevice::SupportsFeatureLevel(FeatureLevel) const {
    return true;
}

void PhysicalDevice::SetupBackendAdapterToggles(TogglesState* adapterToggles) const {}

void PhysicalDevice::SetupBackendDeviceToggles(TogglesState* deviceToggles) const {}

MaybeError PhysicalDevice::InitializeImpl() {
    return {};
}

void PhysicalDevice::InitializeSupportedFeaturesImpl() {
    EnableFeature(Feature::ShaderF16);
}

MaybeError PhysicalDevice::InitializeSupportedLimitsImpl(CombinedLimits* limits) {
    GetDefaultLimitsForSupportedFeatureLevel(&limits->v1);
    return {};
}

void PhysicalDevice::PopulateBackendProperties(UnpackedPtr<AdapterProperties>& properties) const {}

ResultOrError<Ref<DeviceBase>> PhysicalDevice::CreateDeviceImpl(
    AdapterBase* adapter,
    const UnpackedPtr<DeviceDescriptor>& descriptor,
    const TogglesState& toggles) {
    return Device::Create(adapter, descriptor, toggles);
}

FeatureValidationResult PhysicalDevice::ValidateFeatureSupportedWithTogglesImpl(
    wgpu::FeatureName feature,
    const TogglesState& toggles) const {
    return {};
}

}  // namespace dawn::native::interpreter
