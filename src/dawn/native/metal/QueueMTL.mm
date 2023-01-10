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

#include "dawn/native/metal/QueueMTL.h"

#include <set>

#include "dawn/common/Math.h"
#include "dawn/native/Buffer.h"
#include "dawn/native/CommandValidation.h"
#include "dawn/native/Commands.h"
#include "dawn/native/DynamicUploader.h"
#include "dawn/native/metal/BufferMTL.h"
#include "dawn/native/metal/CommandBufferMTL.h"
#include "dawn/native/metal/DeviceMTL.h"
#include "dawn/platform/DawnPlatform.h"
#include "dawn/platform/tracing/TraceEvent.h"

namespace dawn::native::metal {

Queue::Queue(Device* device, const QueueDescriptor* descriptor) : QueueBase(device, descriptor) {}

Queue::~Queue() = default;

MaybeError Queue::SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) {
    Device* device = ToBackend(GetDevice());

    DAWN_TRY(device->Tick());

    CommandRecordingContext* commandContext = device->GetPendingCommandContext();

    std::set<const Buffer*> mappableBuffers;

    TRACE_EVENT_BEGIN0(GetDevice()->GetPlatform(), Recording, "CommandBufferMTL::FillCommands");
    for (uint32_t i = 0; i < commandCount; ++i) {
        DAWN_TRY(ToBackend(commands[i])->FillCommands(commandContext));

        const CommandBufferResourceUsage& resourceUsages = commands[i]->GetResourceUsages();
        for (const BufferBase* buffer : resourceUsages.topLevelBuffers) {
            constexpr wgpu::BufferUsage kTransferBufferUsage =
                wgpu::BufferUsage::MapRead | wgpu::BufferUsage::MapWrite;
            if (buffer->GetUsage() & kTransferBufferUsage) {
                mappableBuffers.insert(ToBackend(buffer));
            }
        }
    }
    TRACE_EVENT_END0(GetDevice()->GetPlatform(), Recording, "CommandBufferMTL::FillCommands");

    DAWN_TRY(device->SubmitPendingCommandBuffer());

    // Set the last usage serial for mappable buffers, so MapAsync() can use this serial for the
    // callback.
    for (const Buffer* buffer : mappableBuffers) {
        const_cast<Buffer*>(buffer)->SetLastUsageSerial(device->GetLastSubmittedCommandSerial());
    }

    return {};
}

}  // namespace dawn::native::metal
