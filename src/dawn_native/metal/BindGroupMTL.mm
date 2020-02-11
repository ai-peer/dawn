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

#include "dawn_native/metal/DeviceMTL.h"

namespace dawn_native { namespace metal {

    // static
    ResultOrError<BindGroup*> BindGroup::Create(Device* device,
                                                const BindGroupDescriptor* descriptor) {
        BindGroupStorage* storage = device->GetBindGroupAllocator()->Allocate(descriptor);
        void* ptr = ObjectHandleBase::Allocate(device);
        return new (ptr) BindGroup(device, storage);
    }

    BindGroup::BindGroup(Device* device, BindGroupStorage* storage)
        : BindGroupBase(device, storage) {
    }

    BindGroup::~BindGroup() {
        if (mStorage != nullptr) {
            static_cast<BindGroupStorage*>(mStorage)->~BindGroupStorage();
            ToBackend(GetDevice())
                ->GetBindGroupAllocator()
                ->Deallocate(static_cast<BindGroupStorage*>(mStorage));
            mStorage = nullptr;
        }
    }

}}  // namespace dawn_native::metal
