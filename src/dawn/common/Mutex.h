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

#ifndef SRC_DAWN_COMMON_MUTEX_H_
#define SRC_DAWN_COMMON_MUTEX_H_

#include <mutex>

#include "dawn/common/ThreadSafety.h"

class LOCKABLE Mutex {
  public:
    inline void lock() ACQUIRE() { mMutex.lock(); }
    inline void unlock() RELEASE() { mMutex.unlock(); }
    inline bool try_lock() TRY_ACQUIRE(true) { return mMutex.try_lock(); }

  private:
    std::mutex mMutex;
};

#endif  // SRC_DAWN_COMMON_MUTEX_H_
