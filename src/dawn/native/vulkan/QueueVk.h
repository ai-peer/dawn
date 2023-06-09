// Copyright 2018 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_VULKAN_QUEUEVK_H_
#define SRC_DAWN_NATIVE_VULKAN_QUEUEVK_H_

#include "dawn/common/SerialQueue.h"
#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Queue.h"
#include "dawn/native/vulkan/CommandRecordingContext.h"

#include <queue>
#include <vector>

namespace dawn::native::vulkan {

class Device;

class Queue final : public QueueBase {
  public:
    static ResultOrError<Ref<Queue>> Create(Device* device,
                                            const QueueDescriptor* descriptor,
                                            uint32_t family,
                                            VkQueue vkQueue);

    VkQueue GetVkQueue() const;

    CommandRecordingContext* GetPendingRecordingContext(SubmitMode submitMode = SubmitMode::Normal);
    MaybeError SplitRecordingContext(CommandRecordingContext* recordingContext);
    MaybeError SubmitPendingCommands();

    void RecycleCompletedCommands();
    void Destroy();

  private:
    Queue(Device* device, const QueueDescriptor* descriptor, uint32_t family, VkQueue vkQueue);
    ~Queue() override;
    using QueueBase::QueueBase;

    MaybeError Initialize();

    MaybeError SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) override;
    bool HasPendingCommands() const override;
    ResultOrError<ExecutionSerial> CheckAndUpdateCompletedSerials() override;
    void ForceEventualFlushOfCommands() override;
    MaybeError WaitForIdleForDestruction() override;

    // Dawn API
    void SetLabelImpl() override;

    ResultOrError<VkFence> GetUnusedFence();

    // We track which operations are in flight on the GPU with an increasing serial.
    // This works only because we have a single queue. Each submit to a queue is associated
    // to a serial and a fence, such that when the fence is "ready" we know the operations
    // have finished.
    std::queue<std::pair<VkFence, ExecutionSerial>> mFencesInFlight;
    // Fences in the unused list aren't reset yet.
    std::vector<VkFence> mUnusedFences;

    MaybeError PrepareRecordingContext();
    ResultOrError<CommandPoolAndBuffer> BeginVkCommandBuffer();

    SerialQueue<ExecutionSerial, CommandPoolAndBuffer> mCommandsInFlight;
    // Command pools in the unused list haven't been reset yet.
    std::vector<CommandPoolAndBuffer> mUnusedCommands;
    // There is always a valid recording context stored in mRecordingContext
    CommandRecordingContext mRecordingContext;

    uint32_t mQueueFamily = 0;
    VkQueue mQueue = VK_NULL_HANDLE;
};

}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_QUEUEVK_H_
