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
#include "dawn_native/ErrorInjector.h"

constexpr VkResult VK_FAKE_ERROR_FOR_TESTING = VK_RESULT_MAX_ENUM;
constexpr VkResult VK_FAKE_DEVICE_OOM_FOR_TESTING = static_cast<VkResult>(VK_RESULT_MAX_ENUM - 1);

namespace dawn_native { namespace vulkan {

    // Returns a string version of the result.
    const char* VkResultAsString(VkResult result);

    MaybeError CheckVkSuccessImpl(VkResult result, const char* context);
    MaybeError CheckVkOOMThenSuccessImpl(VkResult result, const char* context);

#define INJECT_VK_ERROR_OR_RUN(stmt, ...) \
    ::dawn_native::vulkan::VkResult::WrapUnsafe(INJECT_ERROR_OR_RUN(stmt, ##__VA_ARGS__))

// Returns a success only if result if VK_SUCCESS, an error with the context and stringified
// result value instead. Can be used like this:
//
//   DAWN_TRY(CheckVkSuccess(vkDoSomething, "doing something"));
#define CheckVkSuccess(resultIn, contextIn) \
    CheckVkSuccessImpl(INJECT_VK_ERROR_OR_RUN(resultIn, VK_FAKE_ERROR_FOR_TESTING), contextIn)

#define CheckVkOOMThenSuccess(resultIn, contextIn)                                             \
    CheckVkOOMThenSuccessImpl(INJECT_VK_ERROR_OR_RUN(resultIn, VK_FAKE_DEVICE_OOM_FOR_TESTING, \
                                                     VK_FAKE_ERROR_FOR_TESTING),               \
                              contextIn)

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_VULKANERROR_H_
