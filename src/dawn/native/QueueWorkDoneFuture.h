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

#ifndef SRC_DAWN_NATIVE_QUEUEWORKDONEFUTURE_H_
#define SRC_DAWN_NATIVE_QUEUEWORKDONEFUTURE_H_

#include <mutex>
#include <string>

#include "dawn/native/Forward.h"
#include "dawn/native/Future.h"
#include "dawn/webgpu_cpp.h"

namespace dawn::native {

class QueueWorkDoneFutureBase : public FutureBase {
  public:
    explicit QueueWorkDoneFutureBase(QueueBase*);
    ~QueueWorkDoneFutureBase() override = default;

    ObjectType GetType() const override;
    void DestroyImpl() override;

    wgpu::WaitStatus Wait(uint64_t timeout) override;

    // Dawn API
    WGPUQueueWorkDoneResult const* APIGetResult();
    void APIThen(wgpu::CallbackMode, WGPUQueueWorkDoneFutureCallback, void* userdata);

  private:
    Ref<QueueBase> mQueue;
    WGPUQueueWorkDoneFutureCallback mCallback = nullptr;
    void* mUserdata = nullptr;

    void CallCallbackIfAny();
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_QUEUEWORKDONEFUTURE_H_
