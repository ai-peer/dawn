// Copyright 2020 The Dawn Authors
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

#include "dawn_native/metal/BindGroupMTL.h"

#include "common/Math.h"
#include "dawn_native/metal/BindGroupLayoutMTL.h"
#include "dawn_native/metal/Forward.h"
namespace dawn_native { namespace metal {

    BindGroup::BindGroup(DeviceBase* device, const BindGroupDescriptor* descriptor)
        : BindGroupBase(device,
                        descriptor,
                        AlignPtr(reinterpret_cast<char*>(this) + sizeof(BindGroup),
                                 GetBindingDataAlignment())) {
    }

    BindGroup::~BindGroup() {
        ToBackend(GetLayout())->DeallocateBindGroup(this);
    }

    // static
    size_t BindGroup::GetAllocationSize(const BindGroupLayout* layout) {
        return Align(sizeof(BindGroup), GetBindingDataAlignment()) + GetBindingDataSize(layout);
    }

    // static
    BindGroup* BindGroup::Create(DeviceBase* device, const BindGroupDescriptor* descriptor) {
        return ToBackend(descriptor->layout)->AllocateBindGroup(device, descriptor);
    }

}}  // namespace dawn_native::metal
