// Copyright 2020 The Dawn Authors
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

#ifndef DAWNNATIVE_VULKAN_VULKANHANDLES_H_
#define DAWNNATIVE_VULKAN_VULKANHANDLES_H_

#include "common/Platform.h"
#include "common/vulkan_platform.h"

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
    }  // namespace detail

    static constexpr std::nullptr_t VK_NULL_HANDLE = nullptr;

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

    template <typename Tag, typename HandleType>
    HandleType* AsVkArray(VkHandle<Tag, HandleType>* handle) {
        return reinterpret_cast<HandleType*>(handle);
    }

#define DAWN_VK_HANDLE(object)                               \
    using object = VkHandle<struct VkTag##object, ::object>; \
    static_assert(sizeof(object) == sizeof(::object), "");   \
    static_assert(alignof(object) == detail::kNativeVkHandleAlignment, "");
    DAWN_VK_HANDLE(VkInstance)
    DAWN_VK_HANDLE(VkPhysicalDevice)
    DAWN_VK_HANDLE(VkDevice)
    DAWN_VK_HANDLE(VkQueue)
    DAWN_VK_HANDLE(VkCommandBuffer)

#define DAWN_VK_HANDLE64(object)                           \
    DAWN_VK_HANDLE(object)                                 \
    static_assert(sizeof(object) == sizeof(uint64_t), ""); \
    static_assert(alignof(object) == detail::kUint64Alignment, "");
    DAWN_VK_HANDLE64(VkSemaphore)
    DAWN_VK_HANDLE64(VkFence)
    DAWN_VK_HANDLE64(VkDeviceMemory)
    DAWN_VK_HANDLE64(VkBuffer)
    DAWN_VK_HANDLE64(VkImage)
    DAWN_VK_HANDLE64(VkEvent)
    DAWN_VK_HANDLE64(VkQueryPool)
    DAWN_VK_HANDLE64(VkBufferView)
    DAWN_VK_HANDLE64(VkImageView)
    DAWN_VK_HANDLE64(VkShaderModule)
    DAWN_VK_HANDLE64(VkPipelineCache)
    DAWN_VK_HANDLE64(VkPipelineLayout)
    DAWN_VK_HANDLE64(VkRenderPass)
    DAWN_VK_HANDLE64(VkPipeline)
    DAWN_VK_HANDLE64(VkDescriptorSetLayout)
    DAWN_VK_HANDLE64(VkSampler)
    DAWN_VK_HANDLE64(VkDescriptorPool)
    DAWN_VK_HANDLE64(VkDescriptorSet)
    DAWN_VK_HANDLE64(VkFramebuffer)
    DAWN_VK_HANDLE64(VkCommandPool)
    DAWN_VK_HANDLE64(VkSamplerYcbcrConversion)
    DAWN_VK_HANDLE64(VkDescriptorUpdateTemplate)
    DAWN_VK_HANDLE64(VkSurfaceKHR)
    DAWN_VK_HANDLE64(VkSwapchainKHR)
    DAWN_VK_HANDLE64(VkDisplayKHR)
    DAWN_VK_HANDLE64(VkDisplayModeKHR)
    DAWN_VK_HANDLE64(VkDebugReportCallbackEXT)
    DAWN_VK_HANDLE64(VkObjectTableNVX)
    DAWN_VK_HANDLE64(VkIndirectCommandsLayoutNVX)
    DAWN_VK_HANDLE64(VkDebugUtilsMessengerEXT)
    DAWN_VK_HANDLE64(VkValidationCacheEXT)
    DAWN_VK_HANDLE64(VkAccelerationStructureNV)
#undef DAWN_VK_HANDLE64
#undef DAWN_VK_HANDLE

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_VULKANHANDLES_H_
