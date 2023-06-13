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

#ifndef SRC_DAWN_NATIVE_TRACKEDFUTURE_H_
#define SRC_DAWN_NATIVE_TRACKEDFUTURE_H_

#include <atomic>

#include "dawn/common/Ref.h"
#include "dawn/common/RefCounted.h"
#include "dawn/native/Error.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/native/OSEvent.h"
#include "dawn/webgpu_cpp.h"

namespace dawn::native {

class DeviceBase;
class InstanceBase;

class TrackedFuture : public RefCounted {
  public:
    TrackedFuture(InstanceBase* instance, wgpu::CallbackMode callbackMode);
    ~TrackedFuture() override;

    FutureID GetID() const;
    OSEventPrimitive::T GetPrimitive() const;
    void EnsureComplete();

    virtual DeviceBase* GetWaitDevice() const = 0;

  protected:
    virtual void Complete() = 0;

    OSEventReceiver mReceiver;

  private:
    Ref<InstanceBase> mInstance;
    FutureID mFutureID;
    wgpu::CallbackMode mCallbackMode;  // FIXME: actually use this
    std::atomic<bool> mCompleted = false;
};

struct TrackedFuturePollInfo {
    Ref<TrackedFuture> future;
    size_t indexInInfos;
    bool ready;
};

class WorkDoneFuture final : public TrackedFuture {
  public:
    static ResultOrError<Ref<WorkDoneFuture>> Create(DeviceBase* device,
                                                     wgpu::CallbackMode callbackMode,
                                                     WGPUQueueWorkDoneCallback callback,
                                                     void* userdata);

    WorkDoneFuture(DeviceBase* device,
                   wgpu::CallbackMode callbackMode,
                   WGPUQueueWorkDoneCallback callback,
                   void* userdata);
    ~WorkDoneFuture() override = default;

    DeviceBase* GetWaitDevice() const override;

  private:
    void Complete() override;

    Ref<DeviceBase> mDevice;
    ExecutionSerial mSerial;
    WGPUQueueWorkDoneStatus mStatus;
    WGPUQueueWorkDoneCallback mCallback;
    void* mUserdata;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TRACKEDFUTURE_H_
