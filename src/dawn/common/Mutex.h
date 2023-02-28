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

#include <atomic>
#include <mutex>
#include <thread>

#include "dawn/common/Assert.h"
#include "dawn/common/NonCopyable.h"

namespace dawn {
class Mutex : NonCopyable {
  public:
    struct AutoLock : NonCopyable {
        AutoLock() = delete;
        explicit AutoLock(Mutex& mutex) : mMutex(mutex) { mMutex.Lock(); }
        AutoLock(AutoLock&& src) = delete;
        ~AutoLock() { mMutex.Unlock(); }

        AutoLock& operator=(AutoLock&& src) = delete;

      private:
        Mutex& mMutex;
    };

    void Lock();
    void Unlock();

    // This method is only enabled when DAWN_ENABLE_ASSERTS is turned on.
    // Thus it should only be wrapped inside ASSERT() macro.
    // i.e. ASSERT(mutex.IsLockedByCurrentThread())
    bool IsLockedByCurrentThread();

  private:
#if defined(DAWN_ENABLE_ASSERTS)
    std::atomic<std::thread::id> mOwner;
#endif  // DAWN_ENABLE_ASSERTS
    std::mutex mNativeMutex;
};

}  // namespace dawn

#endif
