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

#include "dawn/native/BackendConnection.h"

#include "dawn/common/Log.h"
#include "dawn/native/Instance.h"

namespace dawn::native {

BackendConnection::BackendConnection(InstanceBase* instance, wgpu::BackendType type)
    : mInstance(instance), mType(type) {}

wgpu::BackendType BackendConnection::GetType() const {
    return mType;
}

InstanceBase* BackendConnection::GetInstance() const {
    return mInstance;
}

ResultOrError<std::vector<Ref<AdapterBase>>> BackendConnection::DiscoverAdapters(
    const AdapterDiscoveryOptionsBase* options,
    const TogglesState& adapterToggles) {
    return DAWN_VALIDATION_ERROR("DiscoverAdapters not implemented for this backend.");
}

TogglesState BackendConnection::GenerateInstanceInheritedAdapterToggles(
    const TogglesState& requiredAdapterToggle) const {
    TogglesState adapterToggleStates = requiredAdapterToggle;
    // Inherit from instance toggles
    const TogglesState& instanceTogglesState = mInstance->GetInstanceTogglesState();
    for (uint32_t i : IterateBitSet(instanceTogglesState.togglesIsSet.toggleBitset)) {
        const Toggle& toggle = static_cast<Toggle>(i);
        if (instanceTogglesState.IsEnabled(toggle)) {
            // Enable the toggle if it is enabled in instance toggles set and not disabled in
            // adapter toggle descriptor
            if (!adapterToggleStates.IsDisabled(toggle)) {
                adapterToggleStates.SetIfNotAlready(toggle, true);
            }
        } else {
            // Disable a toggle if it is disabled in instance toggle set, give a warning if it is
            // enabled in adapter toggle descriptor
            if (adapterToggleStates.IsEnabled(toggle)) {
                WarningLog() << "Disabling adapter toggle " << ToggleEnumToName(toggle)
                             << " inherited from instance toggles, which is enabled in adapter "
                                "toggles descriptor.";
                adapterToggleStates.togglesIsEnabled.Set(toggle, false);
            } else {
                adapterToggleStates.SetIfNotAlready(toggle, false);
            }
        }
    }
    return adapterToggleStates;
}

}  // namespace dawn::native
