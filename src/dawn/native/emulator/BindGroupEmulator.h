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

#ifndef SRC_DAWN_NATIVE_EMULATOR_BINDGROUPEMULATOR_H_
#define SRC_DAWN_NATIVE_EMULATOR_BINDGROUPEMULATOR_H_

#include "dawn/native/BindGroup.h"

#include <memory>

namespace dawn::native::emulator {

// Helper class so |BindGroup| can allocate memory for its binding data, before calling the
// BindGroupBase base class constructor.
class BindGroupDataHolder {
  protected:
    explicit BindGroupDataHolder(size_t size) : mBindingDataAllocation(malloc(size)) {}
    ~BindGroupDataHolder() { free(mBindingDataAllocation); }

    void* mBindingDataAllocation;
};

class BindGroup final : private BindGroupDataHolder, public BindGroupBase {
  public:
    BindGroup(DeviceBase* device, const BindGroupDescriptor* descriptor)
        : BindGroupDataHolder(descriptor->layout->GetBindingDataSize()),
          BindGroupBase(device, descriptor, mBindingDataAllocation) {}
};

}  // namespace dawn::native::emulator

#endif  // SRC_DAWN_NATIVE_EMULATOR_BINDGROUPEMULATOR_H_
