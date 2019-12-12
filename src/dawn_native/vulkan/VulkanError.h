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

#ifndef DAWNNATIVE_VULKAN_VULKANERROR_H_
#define DAWNNATIVE_VULKAN_VULKANERROR_H_

#include "common/vulkan_platform.h"
#include "dawn_native/Error.h"
#include "dawn_native/vulkan/VulkanErrorInjector.h"

constexpr VkResult VK_FAKE_ERROR_FOR_TESTING = VK_RESULT_MAX_ENUM;

namespace dawn_native { namespace vulkan {

    // Returns a string version of the result.
    const char* VkResultAsString(VkResult result);

    MaybeError CheckVkSuccessImpl(VkResult result,
                                  const char* context,
                                  const char* file,
                                  const char* func,
                                  int line);

// Returns a success only if result if VK_SUCCESS, an error with the context and stringified
// result value instead. Can be used like this:
//
//   DAWN_TRY(CheckVkSuccess(vkDoSomething, "doing something"));
//
// Note: |resultIn| probably calls a Vulkan function. We inject the
// error before using |resultIn| because it may have side effects
// like writing the handle of a created object to a pointer. If there's an error, these side
// effects should not happen.
#define CheckVkSuccess(resultIn, contextIn)                                                        \
    [&](const char* func) -> ::dawn_native::MaybeError {                                           \
        if (::dawn_native::vulkan::VulkanErrorInjector::ShouldInjectError(__FILE__, func,          \
                                                                          __LINE__)) {             \
            return ::dawn_native::vulkan::CheckVkSuccessImpl(VK_FAKE_ERROR_FOR_TESTING, contextIn, \
                                                             __FILE__, func, __LINE__);            \
        }                                                                                          \
        return ::dawn_native::vulkan::CheckVkSuccessImpl(resultIn, contextIn, __FILE__, func,      \
                                                         __LINE__);                                \
    }(__func__)

// Similar to CheckVkSuccess, but first check for OOM and surface it if found.
#define CheckVkOOMThenSuccess(resultIn, contextIn, oomMessageIn)                                   \
    [&](const char* func) -> ::dawn_native::MaybeError {                                           \
        const char* oomMessage = oomMessageIn;                                                     \
        /* First, maybe mock an OOM error */                                                       \
        if (::dawn_native::vulkan::VulkanErrorInjector::ShouldInjectError(__FILE__, func,          \
                                                                          __LINE__)) {             \
            return ::dawn_native::MakeError(InternalErrorType::OutOfMemory, oomMessage, __FILE__,  \
                                            func, __LINE__);                                       \
        }                                                                                          \
        /* Then, maybe mock a general VkError */                                                   \
        if (::dawn_native::vulkan::VulkanErrorInjector::ShouldInjectError(__FILE__, func,          \
                                                                          __LINE__)) {             \
            return ::dawn_native::vulkan::CheckVkSuccessImpl(VK_FAKE_ERROR_FOR_TESTING, contextIn, \
                                                             __FILE__, func, __LINE__);            \
        }                                                                                          \
        VkResult result = resultIn;                                                                \
        if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {                                             \
            return ::dawn_native::MakeError(InternalErrorType::OutOfMemory, oomMessage, __FILE__,  \
                                            func, __LINE__);                                       \
        }                                                                                          \
        return ::dawn_native::vulkan::CheckVkSuccessImpl(result, contextIn, __FILE__, __func__,    \
                                                         __LINE__);                                \
    }(__func__)

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_VULKANERROR_H_
