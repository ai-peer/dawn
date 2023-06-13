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

using FutureID = uint64_t;

class TrackedFuture : public RefCounted {
  public:
    TrackedFuture(InstanceBase* instance, wgpu::CallbackMode callbackMode);
    ~TrackedFuture() override;

    FutureID GetID() const;
    OSEventPrimitive::T GetPrimitive() const;
    void EnsureComplete();

    virtual DeviceBase* GetWaitDevice() const = 0;

    class WaitRef;
    WaitRef TakeWaitRef();

    // A Ref<TrackedFuture>, but with extra ASSERTs.
    class WaitRef {
      public:
        WaitRef(WaitRef&& other) = default;
        WaitRef& operator=(WaitRef&& other) = default;

        explicit WaitRef(TrackedFuture* future);
        ~WaitRef();

        TrackedFuture* operator->();
        const TrackedFuture* operator->() const;

      private:
        Ref<TrackedFuture> mRef;
    };

  protected:
    virtual void Complete() = 0;

    OSEventReceiver mReceiver;

  private:
    Ref<InstanceBase> mInstance;
    FutureID mFutureID;
    wgpu::CallbackMode mCallbackMode;  // FIXME: actually use this
    std::atomic<bool> mCompleted = false;

#if DAWN_ENABLE_ASSERTS
    std::atomic<bool> mCurrentlyBeingWaited;
#endif
};

// TrackedFuture::WaitRef plus a few extra fields needed for WaitAny implementations.
struct TrackedFutureWaitInfo {
    TrackedFuture::WaitRef future;
    size_t indexInInfos;
    bool ready;
};

class WorkDoneFuture final : public TrackedFuture {
  public:
    static ResultOrError<FutureID> Create(DeviceBase* device,
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
