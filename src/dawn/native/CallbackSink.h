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

#include "dawn/common/NonCopyable.h"

#include <functional>
#include <vector>

namespace dawn::native {

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
