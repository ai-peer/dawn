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

#ifndef SRC_DAWN_NATIVE_INTERPRETER_BINDGROUPINTERPRETER_H_
#define SRC_DAWN_NATIVE_INTERPRETER_BINDGROUPINTERPRETER_H_

#include <memory>

#include "dawn/native/BindGroup.h"

namespace dawn::native::interpreter {

class Device;

// Helper class so |BindGroup| can allocate memory for its binding data, before calling the
// BindGroupBase base class constructor.
class BindGroupDataHolder {
  protected:
    explicit BindGroupDataHolder(size_t size);
    ~BindGroupDataHolder();

    void* mBindingDataAllocation;
};

class BindGroup final : private BindGroupDataHolder, public BindGroupBase {
  public:
    static ResultOrError<Ref<BindGroup>> Create(Device* device,
                                                const BindGroupDescriptor* descriptor);

  private:
    BindGroup(DeviceBase* device, const BindGroupDescriptor* descriptor);
};

}  // namespace dawn::native::interpreter

#endif  // SRC_DAWN_NATIVE_INTERPRETER_BINDGROUPINTERPRETER_H_
