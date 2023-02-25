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

#ifndef SRC_DAWN_NATIVE_EMULATOR_QUEUEPIPELINEEMULATOR_H_
#define SRC_DAWN_NATIVE_EMULATOR_QUEUEPIPELINEEMULATOR_H_

#include <unordered_map>
#include <vector>

#include "dawn/native/Queue.h"

namespace dawn::native::emulator {

class Device;

class Queue final : public QueueBase {
  public:
    static Ref<Queue> Create(Device* device, const QueueDescriptor* descriptor);

  private:
    Queue(Device* device, const QueueDescriptor* descriptor);
    ~Queue() override;

    MaybeError SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) override;

    MaybeError Dispatch(Ref<PipelineBase> pipeline,
                        std::unordered_map<uint32_t, Ref<BindGroupBase>>& bindGroups,
                        std::unordered_map<uint32_t, std::vector<uint32_t>>& dynamicOffsets,
                        uint32_t groupsX,
                        uint32_t groupsY,
                        uint32_t groupsZ);
};

}  // namespace dawn::native::emulator

#endif  // SRC_DAWN_NATIVE_EMULATOR_QUEUEPIPELINEEMULATOR_H_
