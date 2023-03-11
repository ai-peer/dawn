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

#ifndef SRC_DAWN_NATIVE_INTERPRETER_COMPUTEPIPELINEINTERPRETER_H_
#define SRC_DAWN_NATIVE_INTERPRETER_COMPUTEPIPELINEINTERPRETER_H_

#include "dawn/native/ComputePipeline.h"

namespace dawn::native::interpreter {

class Device;

class ComputePipeline final : public ComputePipelineBase {
  public:
    static Ref<ComputePipeline> CreateUninitialized(
        Device* device,
        const UnpackedPtr<ComputePipelineDescriptor>& descriptor);

    MaybeError InitializeImpl() override;

  private:
    ~ComputePipeline() override;
    using ComputePipelineBase::ComputePipelineBase;
};

}  // namespace dawn::native::interpreter

#endif  // SRC_DAWN_NATIVE_INTERPRETER_COMPUTEPIPELINEINTERPRETER_H_
