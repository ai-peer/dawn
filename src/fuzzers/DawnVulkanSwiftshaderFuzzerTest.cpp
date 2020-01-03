#include "vulkan/vulkan_core.h"

#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#define GET_GLOBAL_PROC(vkLib, name)                               \
    auto name = reinterpret_cast<PFN_##name>(dlsym(vkLib, #name)); \
    if (name == nullptr) {                                         \
        return 1;                                                  \
    }

#define GET_INSTANCE_PROC(instance, name)                                             \
    auto name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name)); \
    if (name == nullptr) {                                                            \
        return 1;                                                                     \
    }

std::string GetExecutablePath() {
    std::array<char, PATH_MAX> path;
    ssize_t result = readlink("/proc/self/exe", path.data(), PATH_MAX - 1);
    if (result < 0 || static_cast<size_t>(result) >= PATH_MAX - 1) {
        return "";
    }

    path[result] = '\0';
    return path.data();
}

std::string GetExecutableDirectory() {
    std::string exePath = GetExecutablePath();
    size_t lastPathSepLoc = exePath.find_last_of("/");
    return lastPathSepLoc != std::string::npos ? exePath.substr(0, lastPathSepLoc + 1) : "";
}

bool SetEnvironmentVar(const char* variableName, const char* value) {
    return setenv(variableName, value, 1) == 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string fullSwiftshaderICDPath = GetExecutableDirectory() + DAWN_SWIFTSHADER_VK_ICD_JSON;
    if (setenv("VK_ICD_FILENAMES", fullSwiftshaderICDPath.c_str(), 1) != 0) {
        return 1;
    }

    void* vkLib = dlopen("libvulkan.so.1", RTLD_NOW);
    if (vkLib == nullptr) {
        return 1;
    }

    GET_GLOBAL_PROC(vkLib, vkGetInstanceProcAddr);
    GET_GLOBAL_PROC(vkLib, vkCreateInstance);

    uint32_t apiVersion = VK_MAKE_VERSION(1, 0, 0);

    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = nullptr;
    appInfo.applicationVersion = 0;
    appInfo.pEngineName = nullptr;
    appInfo.engineVersion = 0;
    appInfo.apiVersion = apiVersion;

    VkInstanceCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;
    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    {
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS || instance == VK_NULL_HANDLE) {
            return 1;
        }
    }

    GET_INSTANCE_PROC(instance, vkDestroyInstance);

    // Print device names to verify we've found Swiftshader.
    {
        GET_INSTANCE_PROC(instance, vkEnumeratePhysicalDevices);
        GET_INSTANCE_PROC(instance, vkGetPhysicalDeviceProperties);

        uint32_t count = 0;
        VkResult result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            return 1;
        }
        std::vector<VkPhysicalDevice> physicalDevices(count);
        result = vkEnumeratePhysicalDevices(instance, &count, physicalDevices.data());
        if (result != VK_SUCCESS) {
            return 1;
        }

        for (const auto& physicalDevice : physicalDevices) {
            VkPhysicalDeviceProperties properties = {};
            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            printf("%s\n", properties.deviceName);
        }
    }

    // Comment out to prevent crash.
    vkDestroyInstance(instance, nullptr);

    dlclose(vkLib);
    return 0;
}
