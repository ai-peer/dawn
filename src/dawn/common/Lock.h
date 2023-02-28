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

#ifndef SRC_DAWN_COMMON_LOCK_H_
#define SRC_DAWN_COMMON_LOCK_H_

#include "dawn/common/Mutex.h"
#include "dawn/common/ThreadSafety.h"

namespace dawn {

// Lock is a wrapper around std::unique_lock that provides Thread Safety Analysis.
class SCOPED_LOCKABLE Lock {
  public:
    explicit inline Lock(Mutex& m) ACQUIRE(m) : mLock(m) {}
    inline ~Lock() RELEASE() = default;

    Lock(Lock&& rhs) = default;
    Lock& operator=(Lock&& rhs) = default;

  private:
    std::unique_lock<Mutex> mLock;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_LOCK_H_
