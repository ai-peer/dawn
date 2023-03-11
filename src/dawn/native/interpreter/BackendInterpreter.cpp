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

#include "dawn/native/interpreter/BackendInterpreter.h"

#include <utility>
#include <vector>

#include "dawn/native/ChainUtils.h"
#include "dawn/native/interpreter/PhysicalDeviceInterpreter.h"

namespace dawn::native::interpreter {

Backend::Backend(InstanceBase* instance)
    : BackendConnection(instance, wgpu::BackendType::WgslInterpreter) {}

Backend::~Backend() = default;

std::vector<Ref<PhysicalDeviceBase>> Backend::DiscoverPhysicalDevices(
    const UnpackedPtr<RequestAdapterOptions>& options) {
    if (options->forceFallbackAdapter) {
        return {};
    }

    // There is always a single Interpreter device.
    mDevice = AcquireRef(new PhysicalDevice(GetInstance()));
    return {mDevice};
}

void Backend::ClearPhysicalDevices() {
    mDevice = nullptr;
}

size_t Backend::GetPhysicalDeviceCountForTesting() const {
    return mDevice == nullptr ? 0 : 1;
}

BackendConnection* Connect(InstanceBase* instance) {
    return new Backend(instance);
}

}  // namespace dawn::native::interpreter
