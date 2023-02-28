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
    const AdapterDiscoveryOptionsBase* options) {
    return DAWN_VALIDATION_ERROR("DiscoverAdapters not implemented for this backend.");
}

TogglesState BackendConnection::MakeAdapterToggles(
    const DawnTogglesDescriptor* adapterTogglesDescriptor) const {
    // Setup adapter toggles state
    TogglesState adapterToggles =
        TogglesState::CreateFromTogglesDescriptor(adapterTogglesDescriptor, ToggleStage::Adapter);
    adapterToggles.InheritToggles(GetInstance()->GetTogglesState());
    SetupBackendAdapterToggles(&adapterToggles);

    return adapterToggles;
}

void BackendConnection::SetupBackendAdapterToggles(TogglesState* adapterToggles) const {
    // Do no modification by default if the backend don't have specific adapter toggles. Override
    // this function if necessary.
}

}  // namespace dawn::native
