// Copyright 2019 The Dawn Authors
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

#ifndef DAWNNATIVE_VULKAN_VULKANERRORINJECTOR_H_
#define DAWNNATIVE_VULKAN_VULKANERRORINJECTOR_H_

#include "dawn_native/Error.h"

#include <unordered_map>

namespace dawn_native { namespace vulkan {

    class VulkanErrorInjector {
      public:
        VulkanErrorInjector();
        ~VulkanErrorInjector();

        static VulkanErrorInjector* Get();
        static void Set(VulkanErrorInjector* injector);

        DAWN_FORCE_INLINE static bool ShouldInjectError(const char* file,
                                                        const char* func,
                                                        int line) {
            // This path is used only for tests and fuzzing. It is okay for it to be unoptimized.
            return DAWN_UNLIKELY(gInjector != nullptr) &&
                   gInjector->ShouldInjectErrorImpl(file, func, line);
        }

        void InjectErrorAt(size_t callsite, uint64_t index);
        std::unordered_map<size_t, uint64_t> AcquireCallLog();
        void Clear();

      private:
        static VulkanErrorInjector* gInjector;

        bool ShouldInjectErrorImpl(const char* file, const char* func, int line);

        std::unordered_map<size_t, uint64_t> mCallCounts;
        size_t mInjectedCallsiteFailure = 0;
        uint64_t mInjectedCallsiteFailureIndex = 0;
        bool mHasPendingInjectedError = false;
    };

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_VULKANERRORINJECTOR_H_
