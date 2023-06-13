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

#include "dawn/common/Ref.h"
#include "dawn/common/RefCounted.h"
#include "dawn/native/Error.h"
#include "dawn/native/EventPipe.h"
#include "dawn/native/IntegerTypes.h"
#include "dawn/webgpu_cpp.h"

namespace dawn::native {

class DeviceBase;
class InstanceBase;

class TrackedFuture : public RefCounted {
  public:
    TrackedFuture(InstanceBase* instance, wgpu::CallbackFlag callbackFlags);
    ~TrackedFuture() override;

    FutureID GetID() const;
    EventPrimitive::T GetPrimitive() const;
    void Complete();

  protected:
    virtual void CompleteImpl() = 0;

    EventReceiver mReceiver;

  private:
    Ref<InstanceBase> mInstance;
    FutureID mFutureID;
    wgpu::CallbackFlag
        mCallbackFlags;  // FIXME use this to spontaneously run callbacks in some cases
    bool mCompleted = false;
};

class WorkDoneFuture : public TrackedFuture {
  public:
    static ResultOrError<Ref<WorkDoneFuture>> Create(DeviceBase* device,
                                                     wgpu::CallbackFlag callbackFlags,
                                                     WGPUQueueWorkDoneCallback callback,
                                                     void* userdata);

    WorkDoneFuture(DeviceBase* device,
                   wgpu::CallbackFlag callbackFlags,
                   WGPUQueueWorkDoneCallback callback,
                   void* userdata);
    ~WorkDoneFuture() override = default;

  private:
    void CompleteImpl() override;

    Ref<DeviceBase> mDevice;
    ExecutionSerial mSerial;
    WGPUQueueWorkDoneStatus mStatus;
    WGPUQueueWorkDoneCallback mCallback;
    void* mUserdata;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_TRACKEDFUTURE_H_
