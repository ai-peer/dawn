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

#ifndef SRC_DAWN_NATIVE_CALLBACKSINK_H_
#define SRC_DAWN_NATIVE_CALLBACKSINK_H_

#include <functional>
#include <vector>

#include "dawn/common/NonCopyable.h"

namespace dawn::native {

// This class is used to collect callbacks during execution of a function's call stack. And only
// execute them once being out of scope. This is to delay the execution of callbacks as late as
// possible after all the state modifications have been finished or mutex is unlocked. One example
// is when we submit a command buffer:
// - wgpu::Queue::Submit()
//   - mutex.lock()
//      - Queue::SubmitImpl()
//        - Device::Tick()
//          - trigger user's callback
//            - user calls wgpu::Buffer::MapAsync()
//              * mutex.lock()
//
// The second mutex.lock() above is a deadlock. To avoid this, instead of calling the callbacks
// immediately, we add it the CallbackSink which is passed down from API level:
// - wgpu::Queue::Submit()
//   - CallbackSink sink
//   - mutex.lock()
//      - Queue::SubmitImpl(&sink)
//        - Device::Tick(&sink)
//          - sink.Add(callback)
//        - return
//      - return
//   - mutex.unlock()
//   - sink.Drain()
//     - trigger callback
//       - wgpu::Buffer::MapAsync()
//         - mutex.lock()
//         ...
class CallbackSink : public NonCopyable {
  public:
    ~CallbackSink() { Drain(); }

    void Add(const std::function<void()> callback) { mCallbacks.push_back(callback); }

    void Drain() {
        for (auto& callback : mCallbacks) {
            callback();
        }
        mCallbacks.resize(0);
    }

  private:
    std::vector<std::function<void()>> mCallbacks;
};

}  // namespace dawn::native

#endif
