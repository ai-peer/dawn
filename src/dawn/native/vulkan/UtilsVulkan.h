// Copyright 2019 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_DAWN_NATIVE_VULKAN_UTILSVULKAN_H_
#define SRC_DAWN_NATIVE_VULKAN_UTILSVULKAN_H_

#include <string>
#include <vector>

#include "dawn/common/vulkan_platform.h"
#include "dawn/native/Commands.h"
#include "dawn/native/dawn_platform.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace dawn::native {
struct ProgrammableStage;
union OverrideScalar;
}  // namespace dawn::native

namespace dawn::native::vulkan {

class Device;
struct VulkanFunctions;

// A helper function appending |element| to a pNext chain of extension structs started from |head|.
//
// Examples:
//     VkPhysicalFeatures2 features2 = {
//       .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
//       .pNext = nullptr,
//     };
//
//     PNextChainAppend(&features2,
//                      &featuresExtensions.subgroupSizeControl)
//                      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_FEATURES_EXT);
//
template <typename A, typename B>
inline void PNextChainAppend(A* list, B* element) {
    static_assert(offsetof(A, sType) == offsetof(VkBaseOutStructure, sType));
    static_assert(offsetof(B, sType) == offsetof(VkBaseOutStructure, sType));
    static_assert(offsetof(A, pNext) == offsetof(VkBaseOutStructure, pNext));
    static_assert(offsetof(B, pNext) == offsetof(VkBaseOutStructure, pNext));
    // NOTE: Some VK_STRUCT_TYPEs define their pNext field as a const void*
    // which is why the VkBaseOutStructure* casts below are necessary.
    PNextChainAppend(reinterpret_cast<VkBaseOutStructure*>(list),
                     reinterpret_cast<VkBaseOutStructure*>(element));
}

template <>
inline void PNextChainAppend(VkBaseOutStructure* list, VkBaseOutStructure* element) {
    VkBaseOutStructure* last = list;
    while (last->pNext) {
        last = last->pNext;
    }
    last->pNext = element;
    element->pNext = nullptr;
}

// A variant of PNextChainAppend() above that also initializes the |sType| field in |element|.
template <typename A, typename B>
inline void PNextChainAppend(A* list, B* element, VkStructureType sType) {
    element->sType = sType;
    PNextChainAppend(list, element);
}

VkCompareOp ToVulkanCompareOp(wgpu::CompareFunction op);

VkImageAspectFlags VulkanAspectMask(const Aspect& aspects);

Extent3D ComputeTextureCopyExtent(const TextureCopy& textureCopy, const Extent3D& copySize);

VkBufferImageCopy ComputeBufferImageCopyRegion(const BufferCopy& bufferCopy,
                                               const TextureCopy& textureCopy,
                                               const Extent3D& copySize);
VkBufferImageCopy ComputeBufferImageCopyRegion(const TextureDataLayout& dataLayout,
                                               const TextureCopy& textureCopy,
                                               const Extent3D& copySize);

// Gets the associated VkObjectType for any non-dispatchable handle
template <class HandleType>
VkObjectType GetVkObjectType(HandleType handle);

void SetDebugNameInternal(Device* device,
                          VkObjectType objectType,
                          uint64_t objectHandle,
                          const char* prefix,
                          std::string label);

// The majority of Vulkan handles are "non-dispatchable". Dawn wraps these by overriding
// VK_DEFINE_NON_DISPATCHABLE_HANDLE to add some capabilities like making null comparisons
// easier. In those cases we can make setting the debug name a bit easier by getting the
// object type automatically and handling the indirection to the native handle.
template <typename Tag, typename HandleType>
void SetDebugName(Device* device,
                  detail::VkHandle<Tag, HandleType> objectHandle,
                  const char* prefix,
                  std::string label = "") {
    uint64_t handle;
    if constexpr (std::is_same_v<HandleType, uint64_t>) {
        handle = objectHandle.GetHandle();
    } else {
        handle = reinterpret_cast<uint64_t>(objectHandle.GetHandle());
    }
    SetDebugNameInternal(device, GetVkObjectType(objectHandle), handle, prefix, label);
}

// Handles like VkQueue and VKDevice require a special path because they are dispatchable, so
// they require an explicit VkObjectType and cast to a uint64_t directly rather than by getting
// the non-dispatchable wrapper's underlying handle.
template <typename HandleType>
void SetDebugName(Device* device,
                  VkObjectType objectType,
                  HandleType objectHandle,
                  const char* prefix,
                  std::string label = "") {
    SetDebugNameInternal(device, objectType, reinterpret_cast<uint64_t>(objectHandle), prefix,
                         label);
}

std::string GetNextDeviceDebugPrefix();
std::string GetDeviceDebugPrefixFromDebugName(const char* debugName);

// Get the properties for the given format.
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDrmFormatModifierPropertiesEXT.html
std::vector<VkDrmFormatModifierPropertiesEXT> GetFormatModifierProps(
    const VulkanFunctions& fn,
    VkPhysicalDevice vkPhysicalDevice,
    VkFormat format);

// Get the properties for the (format, modifier) pair.
// https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkDrmFormatModifierPropertiesEXT.html
ResultOrError<VkDrmFormatModifierPropertiesEXT> GetFormatModifierProps(
    const VulkanFunctions& fn,
    VkPhysicalDevice vkPhysicalDevice,
    VkFormat format,
    uint64_t modifier);
}  // namespace dawn::native::vulkan

#endif  // SRC_DAWN_NATIVE_VULKAN_UTILSVULKAN_H_
