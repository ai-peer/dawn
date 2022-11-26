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

#include "dawn/native/emulator/AdapterEmulator.h"

#include "dawn/native/ErrorData.h"
#include "dawn/native/Instance.h"
#include "dawn/native/emulator/DeviceEmulator.h"

namespace dawn::native::emulator {

Adapter::Adapter(InstanceBase* instance) : AdapterBase(instance, wgpu::BackendType::Emulator) {
    mVendorId = 0;
    mDeviceId = 0;
    mName = "Emulator backend";
    mAdapterType = wgpu::AdapterType::CPU;
    MaybeError err = Initialize();
    ASSERT(err.IsSuccess());
}

Adapter::~Adapter() = default;

bool Adapter::SupportsExternalImages() const {
    return false;
}

MaybeError Adapter::InitializeImpl() {
    return {};
}

void Adapter::InitializeSupportedFeaturesImpl() {}

MaybeError Adapter::InitializeSupportedLimitsImpl(CombinedLimits* limits) {
    GetDefaultLimits(&limits->v1);
    return {};
}

ResultOrError<Ref<DeviceBase>> Adapter::CreateDeviceImpl(
    const DeviceDescriptor* descriptor,
    const TripleStateTogglesSet& userProvidedToggles) {
    return Device::Create(this, descriptor, userProvidedToggles);
}

MaybeError Adapter::ValidateFeatureSupportedWithTogglesImpl(
    wgpu::FeatureName feature,
    const TripleStateTogglesSet& userProvidedToggles) {
    return {};
}

}  // namespace dawn::native::emulator
