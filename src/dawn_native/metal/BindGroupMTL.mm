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
        return new BindGroup(device, device->GetBindGroupAllocator()->Allocate(descriptor));
    }

    BindGroup::~BindGroup() {
        BindGroupStorage* storage = static_cast<BindGroupStorage*>(mStorage.release());
        storage->~BindGroupStorage();
        ToBackend(GetDevice())->GetBindGroupAllocator()->Deallocate(storage);
    }

}}  // namespace dawn_native::metal
