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

#include "dawn/native/interpreter/ComputePipelineInterpreter.h"

#include "dawn/native/ErrorData.h"
#include "dawn/native/interpreter/DeviceInterpreter.h"

namespace dawn::native::interpreter {

// static
Ref<ComputePipeline> ComputePipeline::CreateUninitialized(
    Device* device,
    const UnpackedPtr<ComputePipelineDescriptor>& descriptor) {
    return AcquireRef(new ComputePipeline(device, descriptor));
}

MaybeError ComputePipeline::InitializeImpl() {
    return {};
}

ComputePipeline::~ComputePipeline() = default;

}  // namespace dawn::native::interpreter
