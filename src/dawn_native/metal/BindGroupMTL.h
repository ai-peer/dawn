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

#ifndef DAWNNATIVE_METAL_BINDGROUPMTL_H_
#define DAWNNATIVE_METAL_BINDGROUPMTL_H_

#include "common/PlacementAllocated.h"
#include "dawn_native/BindGroup.h"

namespace dawn_native { namespace metal {

    class Device;

    class alignas(64) BindGroupStorage : public BindGroupStorageBase, public PlacementAllocated {
      public:
        using BindGroupStorageBase::BindGroupStorageBase;
    };

    class BindGroup : public BindGroupBase {
      public:
        static ResultOrError<BindGroup*> Create(Device* device,
                                                const BindGroupDescriptor* descriptor);
        ~BindGroup() override;

      private:
        BindGroup(Device* device, BindGroupStorage* storage);
    };

}}  // namespace dawn_native::metal

#endif  // DAWNNATIVE_METAL_BINDGROUPMTL_H_
