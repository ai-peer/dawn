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

#include "dawn/native/emulator/BackendEmulator.h"

#include "dawn/native/emulator/AdapterEmulator.h"

namespace dawn::native::emulator {

Backend::Backend(InstanceBase* instance)
    : BackendConnection(instance, wgpu::BackendType::Emulator) {}

Backend::~Backend() = default;

std::vector<Ref<AdapterBase>> Backend::DiscoverDefaultAdapters() {
    // There is always a single Emulator adapter.
    std::vector<Ref<AdapterBase>> adapters;
    Ref<Adapter> adapter = AcquireRef(new Adapter(GetInstance()));
    adapters.push_back(std::move(adapter));
    return adapters;
}

BackendConnection* Connect(InstanceBase* instance) {
    return new Backend(instance);
}

}  // namespace dawn::native::emulator
