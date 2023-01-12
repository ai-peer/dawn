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

#include "dawn/native/vulkan/QueueVk.h"

#include <set>

#include "dawn/common/Math.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/CommandValidation.h"
#include "dawn/native/Commands.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/vulkan/BufferVk.h"
#include "dawn/native/vulkan/CommandBufferVk.h"
#include "dawn/native/vulkan/CommandRecordingContext.h"
#include "dawn/native/vulkan/DeviceVk.h"
#include "dawn/native/vulkan/UtilsVulkan.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::vulkan {

// static
Ref<Queue> Queue::Create(Device* device, const QueueDescriptor* descriptor) {
    Ref<Queue> queue = AcquireRef(new Queue(device, descriptor));
    queue->Initialize();
    return queue;
}

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

Queue::~Queue() {}

void Queue::Initialize() {
    SetLabelImpl();
}

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    Device* device = ToBackend(GetDevice());

    DAWN_TRY(device->Tick());
    constexpr wgpu::BufferUsage kTransferBufferUsage =
        wgpu::BufferUsage::MapRead | wgpu::BufferUsage::MapWrite;

    TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferVk::RecordCommands");
    CommandRecordingContext* recordingContext = device->GetPendingRecordingContext();
    std::set<const Buffer*> mappableBuffers;
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ToBackend(commands[i])->RecordCommands(recordingContext));
        const CommandBufferResourceUsage& resourceUsages = commands[i]->GetResourceUsages();
        for (const BufferBase* buffer : resourceUsages.topLevelBuffers) {
            if (buffer->GetUsage() & kTransferBufferUsage) {
                mappableBuffers.insert(ToBackend(buffer));
            }
        }
    }
    TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferVk::RecordCommands");

    for (const Buffer* buffer : mappableBuffers) {
        // Prepare transfer buffers for the next MapAsync() call here, so MapAsync() call doesn't
        // need an extra queue submission.
        const_cast<Buffer*>(buffer)->TransitionUsageNow(recordingContext,
                                                        buffer->GetUsage() & kTransferBufferUsage);
        // TransitionUsageNow() should update the last usage serial.
        ASSERT(buffer->GetLastUsageSerial() == device->GetPendingCommandSerial());

    }

    return device->SubmitPendingCommands();
}

void Queue::SetLabelImpl() {
    Device* device = ToBackend(GetDevice());
    // TODO(crbug.com/dawn/1344): When we start using multiple queues this needs to be adjusted
    // so it doesn't always change the default queue's label.
    SetDebugName(device, VK_OBJECT_TYPE_QUEUE, device->GetQueue(), "Dawn_Queue", GetLabel());
}

}  // namespace dawn::native::vulkan
