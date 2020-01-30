// Copyright 2017 The Dawn Authors
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

#ifndef COMMON_VULKANPLATFORM_H_
#define COMMON_VULKANPLATFORM_H_

#if !defined(DAWN_ENABLE_BACKEND_VULKAN)
#    error "vulkan_platform.h included without the Vulkan backend enabled"
#endif
#if defined(VULKAN_CORE_H_)
#    error "vulkan.h included before vulkan_platform.h"
#endif

#include "common/Platform.h"

#include <cstddef>
#include <cstdint>

namespace dawn_native { namespace vulkan {

    namespace detail {
        template <typename T>
        struct WrapperStruct {
            T member;
        };

        template <typename T>
        static constexpr size_t AlignOfInStruct = alignof(WrapperStruct<T>);

#if defined(DAWN_PLATFORM_64_BIT)
        typedef struct VkSomeHandle_T* VkSomeHandle;
#elif defined(DAWN_PLATFORM_32_BIT)
        typedef uint64_t VkSomeHandle;
#else
#    error "Unsupported platform"
#endif

        static constexpr size_t kNativeVkHandleAlignment = AlignOfInStruct<VkSomeHandle>;
        static constexpr size_t kUint64Alignment = AlignOfInStruct<uint64_t>;

        // Simple handle types that supports "nullptr_t" as a 0 value.
        template <typename Tag, typename HandleType>
        class alignas(detail::kNativeVkHandleAlignment) VkHandle {
          public:
            // Default constructor and assigning of VK_NULL_HANDLE
            VkHandle() = default;
            VkHandle(std::nullptr_t) {
            }

            // Use default copy constructor/assignment
            VkHandle(const VkHandle<Tag, HandleType>& other) = default;
            VkHandle& operator=(const VkHandle<Tag, HandleType>&) = default;

            // Comparisons between handles
            bool operator==(VkHandle<Tag, HandleType> other) const {
                return mHandle == other.mHandle;
            }
            bool operator!=(VkHandle<Tag, HandleType> other) const {
                return mHandle != other.mHandle;
            }

            // Comparisons between handles and VK_NULL_HANDLE
            bool operator==(std::nullptr_t) const {
                return mHandle == 0;
            }
            bool operator!=(std::nullptr_t) const {
                return mHandle != 0;
            }

            // Implicit conversion to real Vulkan types.
            operator HandleType() const {
                return GetHandle();
            }

            HandleType GetHandle() const {
                return mHandle;
            }

            HandleType& operator*() {
                return mHandle;
            }

            static VkHandle<Tag, HandleType> CreateFromHandle(HandleType handle) {
                return VkHandle{handle};
            }

          private:
            explicit VkHandle(HandleType handle) : mHandle(handle) {
            }

            HandleType mHandle = 0;
        };
    }  // namespace detail

    static constexpr std::nullptr_t VK_NULL_HANDLE = nullptr;

    template <typename Tag, typename HandleType>
    HandleType* AsVkArray(detail::VkHandle<Tag, HandleType>* handle) {
        return reinterpret_cast<HandleType*>(handle);
    }

}}  // namespace dawn_native::vulkan

#define DAWN_VK_HANDLE64(object)                                                    \
    namespace dawn_native { namespace vulkan {                                      \
            using object = detail::VkHandle<struct VkTag##object, ::object>;        \
            static_assert(sizeof(object) == sizeof(::object), "");                  \
            static_assert(alignof(object) == detail::kNativeVkHandleAlignment, ""); \
            static_assert(sizeof(object) == sizeof(uint64_t), "");                  \
            static_assert(alignof(object) == detail::kUint64Alignment, "");         \
        }                                                                           \
    }  // namespace dawn_native::vulkan

#if defined(DAWN_PLATFORM_64_BIT)
#    define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) \
        typedef struct object##_T* object;            \
        DAWN_VK_HANDLE64(object)
#elif defined(DAWN_PLATFORM_32_BIT)
#    define VK_DEFINE_NON_DISPATCHABLE_HANDLE(object) \
        typedef uint64_t object;                      \
        DAWN_VK_HANDLE64(object)
#else
#    error "Unsupported platform"
#endif

#include <vulkan/vulkan.h>

// Redefine VK_NULL_HANDLE for better type safety where possible.
#undef VK_NULL_HANDLE
#if defined(DAWN_PLATFORM_64_BIT)
static constexpr nullptr_t VK_NULL_HANDLE = nullptr;
#elif defined(DAWN_PLATFORM_32_BIT)
static constexpr uint64_t VK_NULL_HANDLE = 0;
#else
#    error "Unsupported platform"
#endif

// Remove windows.h macros after vulkan_platform's include of windows.h
#if defined(DAWN_PLATFORM_WINDOWS)
#    include "common/windows_with_undefs.h"
#endif
// Remove X11/Xlib.h macros after vulkan_platform's include of it.
#if defined(DAWN_USE_X11)
#    include "common/xlib_with_undefs.h"
#endif

// Include Fuchsia-specific definitions that are not upstreamed yet.
#if defined(DAWN_PLATFORM_FUCHSIA)
#    include <vulkan/vulkan_fuchsia_extras.h>
#endif

#endif  // COMMON_VULKANPLATFORM_H_
