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

#ifndef SRC_DAWN_NATIVE_D3D11_QUEUED3D11_H_
#define SRC_DAWN_NATIVE_D3D11_QUEUED3D11_H_

#include "dawn/native/Queue.h"

#include "dawn/native/d3d11/CommandRecordingContextD3D11.h"
#include "dawn/native/d3d11/Forward.h"

namespace dawn::native::d3d11 {

class Device;

class Queue final : public QueueBase {
  public:
    static ResultOrError<Ref<Queue>> Create(Device* device, const QueueDescriptor* descriptor);

    CommandRecordingContext* GetPendingCommandContext(SubmitMode submitMode = SubmitMode::Normal);
    MaybeError SubmitPendingCommands();
    ID3D11Fence* GetFence() const;
    void Destroy();
    MaybeError NextSerial();
    MaybeError WaitForSerial(ExecutionSerial serial);

  private:
    using QueueBase::QueueBase;

    ~Queue() override = default;

    MaybeError Initialize();

    MaybeError SubmitImpl(uint32_t commandCount, CommandBufferBase* const* commands) override;
    MaybeError WriteBufferImpl(BufferBase* buffer,
                               uint64_t bufferOffset,
                               const void* data,
                               size_t size) override;
    MaybeError WriteTextureImpl(const ImageCopyTexture& destination,
                                const void* data,
                                const TextureDataLayout& dataLayout,
                                const Extent3D& writeSizePixel) override;

    bool HasPendingCommands() const override;
    ResultOrError<ExecutionSerial> CheckAndUpdateCompletedSerials() override;
    void ForceEventualFlushOfCommands() override;
    MaybeError WaitForIdleForDestruction() override;

    ComPtr<ID3D11Fence> mFence;
    HANDLE mFenceEvent = nullptr;
    CommandRecordingContext mPendingCommands;
};

}  // namespace dawn::native::d3d11

#endif  // SRC_DAWN_NATIVE_D3D11_QUEUED3D11_H_
