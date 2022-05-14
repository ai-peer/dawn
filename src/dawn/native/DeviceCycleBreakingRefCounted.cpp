// Copyright 2022 The Dawn Authors
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

#include "dawn/native/DeviceCycleBreakingRefCounted.h"

namespace dawn::native {

void DeviceCycleBreakingRefCounted::Externalize() {
    uint64_t previousExternalRefCount = mExternalRefCount.fetch_add(1, std::memory_order_relaxed);
    ASSERT(previousExternalRefCount == 0u);
#if defined(DAWN_ENABLE_ASSERTS)
    ASSERT(!mExternalized);
    mExternalized = true;
#endif
}

void DeviceCycleBreakingRefCounted::APIReference() {
#if defined(DAWN_ENABLE_ASSERTS)
    ASSERT(mExternalized);
#endif
    mExternalRefCount.fetch_add(1, std::memory_order_relaxed);
    RefCounted::APIReference();
}

void DeviceCycleBreakingRefCounted::APIRelease() {
#if defined(DAWN_ENABLE_ASSERTS)
    ASSERT(mExternalized);
#endif
    uint64_t previousRefCount = mExternalRefCount.fetch_sub(1, std::memory_order_release);
    if (previousRefCount == 1u) {
        std::atomic_thread_fence(std::memory_order_acquire);
        WillDropLastExternalRef();
    }
    Release();
}

}  // namespace dawn::native
