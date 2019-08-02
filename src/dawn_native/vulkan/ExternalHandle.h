#ifndef DAWNNATIVE_VULKAN_EXTERNALHANDLE_H_
#define DAWNNATIVE_VULKAN_EXTERNALHANDLE_H_

namespace dawn_native { namespace vulkan {

#ifdef DAWN_PLATFORM_LINUX
    using ExternalHandle = int;
#else
    using ExternalHandle = int;  // Placeholder
#endif

}}  // namespace dawn_native::vulkan

#endif  // DAWNNATIVE_VULKAN_EXTERNALHANDLE_H_
