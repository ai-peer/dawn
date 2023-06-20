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

#include "dawn/native/d3d12/IntelExtensionD3D12.h"

#if DAWN_PLATFORM_IS_WIN32

#include <Devpkey.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <psapi.h>
#include <shlwapi.h>

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dawn/common/Assert.h"
#include "dawn/native/d3d12/DeviceD3D12.h"
#include "dawn/native/d3d12/PhysicalDeviceD3D12.h"

#define INTC_IGDEXT_D3D12

#include "third_party/IntelExtension/CmdThrottleExtension/IntelExtension/include/igdext.h"

namespace dawn::native::d3d12 {

namespace {

// Function pointers to load necessary entry points from Intel extension DLL.
using FN_GetSupportedExtensionVersions = HRESULT (*)(ID3D12Device* pDevice,
                                                     INTCExtensionVersion* pVersions,
                                                     UINT32* pVersionsCount);

using FN_CreateDeviceExtensionContext = HRESULT (*)(ID3D12Device* pDevice,
                                                    INTCExtensionContext** ppExtensionContext,
                                                    INTCExtensionInfo* pExtensionInfo,
                                                    INTCExtensionAppInfo* pExtensionAppInfo);

using FN_DestroyDeviceExtensionContext = HRESULT (*)(INTCExtensionContext** ppExtensionContext);

using FN_CreateCommandQueue = HRESULT (*)(INTCExtensionContext* pExtensionContext,
                                          const INTC_D3D12_COMMAND_QUEUE_DESC* pDesc,
                                          REFIID riid,
                                          void** ppCommandQueue);

// Helper functions, definitions and data structures for loading Intel extension DLL.
struct DeviceInfo {
    wchar_t driverPath[MAX_PATH];
    wchar_t deviceID[MAX_DEVICE_ID_LEN];
    DEVINST deviceInstanceHandle = 0;
};

bool FillDeviceID(DeviceInfo* deviceInfo, const PhysicalDevice& adapter) {
    std::array<wchar_t, 40> displayDevClassGUID;
    int32_t resultLength = StringFromGUID2(GUID_DEVCLASS_DISPLAY, displayDevClassGUID.data(),
                                           sizeof(displayDevClassGUID));
    ASSERT(resultLength + 1 == displayDevClassGUID.size());

    ULONG deviceIDListSize = 0;
    if (CM_Get_Device_ID_List_SizeW(&deviceIDListSize, displayDevClassGUID.data(),
                                    CM_GETIDLIST_FILTER_CLASS) != CR_SUCCESS) {
        return false;
    }
    std::vector<wchar_t> deviceIDList(deviceIDListSize);
    if (CM_Get_Device_ID_ListW(displayDevClassGUID.data(), deviceIDList.data(), deviceIDListSize,
                               CM_GETIDLIST_FILTER_CLASS) != CR_SUCCESS) {
        return false;
    }

    std::wstringstream stream;
    stream << "VEN_8086&DEV_" << std::hex << adapter.GetDeviceId();

    bool foundDeviceID = false;
    wchar_t* currentDeviceID = deviceIDList.data();
    do {
        if (wcslen(currentDeviceID) > 0 &&
            wcsstr(currentDeviceID, stream.str().c_str()) != nullptr) {
            wcscpy_s(deviceInfo->deviceID, MAX_DEVICE_ID_LEN, currentDeviceID);
            foundDeviceID = true;
            break;
        }
        // Find next device ID in deviceIDList.
        currentDeviceID = wcschr(currentDeviceID, L'\0') + 1;
    } while (currentDeviceID < deviceIDList.data() + deviceIDListSize);

    return foundDeviceID;
}

bool FillDeviceInstanceHandle(DeviceInfo* deviceInfo) {
    return CM_Locate_DevNodeW(&deviceInfo->deviceInstanceHandle, deviceInfo->deviceID,
                              CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS;
}

bool FillDeviceDriverPath(DeviceInfo* deviceInfo) {
    ULONG propertySize = 0;
    DEVPROPTYPE propertyType;
    if (CM_Get_DevNode_PropertyW(deviceInfo->deviceInstanceHandle, &DEVPKEY_Device_DriverInfPath,
                                 &propertyType, nullptr, &propertySize, 0) != CR_BUFFER_SMALL) {
        return false;
    }
    std::vector<uint8_t> propertyData(propertySize);
    if (CM_Get_DevNode_PropertyW(deviceInfo->deviceInstanceHandle, &DEVPKEY_Device_DriverInfPath,
                                 &propertyType, propertyData.data(), &propertySize,
                                 0) != CR_SUCCESS) {
        return false;
    }
    const wchar_t* propertyInfName = reinterpret_cast<const wchar_t*>(propertyData.data());
    if (!SetupGetInfDriverStoreLocationW(&propertyInfName[0], nullptr, nullptr,
                                         deviceInfo->driverPath, MAX_PATH, nullptr)) {
        return false;
    }
    // Replace '//' with '/'
    PathRemoveFileSpecW(deviceInfo->driverPath);
    return wcslen(deviceInfo->driverPath) > 0;
}

HMODULE LoadIntelExtensionLibrary(const DeviceInfo& deviceInfo) {
    HANDLE currentProcessHandle = GetCurrentProcess();

    // Get all the modules in current process
    uint32_t processModulesCount = 1024;
    std::vector<HMODULE> processModules(processModulesCount);
    DWORD bytesRequiredForAllModules = 0;
    while (true) {
        if (!EnumProcessModules(currentProcessHandle, processModules.data(),
                                sizeof(HMODULE) * processModulesCount,
                                &bytesRequiredForAllModules)) {
            return nullptr;
        }

        if (sizeof(HMODULE) * processModulesCount >= bytesRequiredForAllModules) {
            break;
        }
        processModules.resize(bytesRequiredForAllModules / sizeof(HMODULE));
    }

    // Go through all the enumerated processes and find the loaded driver module
    std::array<wchar_t, MAX_PATH> fullPath;
    bool foundDriverModule = false;
    for (uint32_t i = 0; i < bytesRequiredForAllModules / sizeof(HMODULE); ++i) {
        // Get the full path to the module
        if (!GetModuleFileNameExW(currentProcessHandle, processModules[i], fullPath.data(),
                                  sizeof(fullPath) / sizeof(wchar_t))) {
            continue;
        }

        // Break when we find the driver module we need
        if (wcsstr(fullPath.data(), deviceInfo.driverPath) &&
            wcsstr(fullPath.data(), L"DriverStore\\FileRepository")) {
            foundDriverModule = true;
            break;
        }
    }

    if (!foundDriverModule) {
        return nullptr;
    }

    // For example, suppose fullPath = C:\WINDOWS\System32\DriverStore\FileRepository\xxx\yyy.dll
    // driverFileName = \yyy.dll
    wchar_t* driverFileName = wcsrchr(fullPath.data(), '\\');
    if (driverFileName == nullptr) {
        return nullptr;
    }

    // Get the full path of Intel extension DLL, which should be at the same directory of the Intel
    // driver file.
    size_t driverFileNameLength = wcslen(driverFileName);
    if (driverFileNameLength > 0) {
        // driverFileName = yyy.dll
        ++driverFileName;
    }

#if DAWN_PLATFORM_IS(64_BIT)
    const wchar_t* kIntelD3DExtensionDLL = L"igdext64.dll";
#else
    const wchar_t* kIntelD3DExtensionDLL = L"igdext32.dll";
#endif
    size_t extensionFileNameLength = wcslen(kIntelD3DExtensionDLL);
    size_t fullDriverFilePathLength = wcslen(fullPath.data());
    if (fullDriverFilePathLength - driverFileNameLength + extensionFileNameLength + 1 <= MAX_PATH) {
        // fullPath = C:\WINDOWS\System32\DriverStore\FileRepository\xxx\${kIntelD3DExtensionDLL}
        memcpy_s(driverFileName, MAX_PATH, kIntelD3DExtensionDLL,
                 (extensionFileNameLength + 1) * sizeof(wchar_t));
        return LoadLibraryExW(fullPath.data(), nullptr, 0);
    } else {
        // We should only load the DLL from the directory where the current driver file is stored.
        return nullptr;
    }
}

}  // anonymous namespace

class IntelExtensionImpl : public IntelExtension {
  public:
    explicit IntelExtensionImpl(const PhysicalDevice& physicalDevice);
    ~IntelExtensionImpl() override;

    HRESULT CreateCommandQueueWithMaxPerformanceThrottlePolicy(
        D3D12_COMMAND_QUEUE_DESC* d3d12CommandQueueDesc,
        REFIID riid,
        void** ppCommandQueue) override;

    bool IsThrottlePolicyExtensionSupported() const;

  private:
    bool LoadInterfaces();
    bool CreateINTCExtensionContext(const PhysicalDevice& physicalDevice);

    void CleanUp();

    HMODULE mIntelExtensionDLLModule = nullptr;

    INTCExtensionContext* mINTCExtensionContext = nullptr;

    struct IntelExtensionInterfaces {
        // Interfaces required by all extensions
        FN_CreateDeviceExtensionContext CreateDeviceExtensionContext = nullptr;
        FN_DestroyDeviceExtensionContext DestroyDeviceExtensionContext = nullptr;
        FN_GetSupportedExtensionVersions GetSupportedVersions = nullptr;

        // Interfaces for Throttle Policy Extension
        FN_CreateCommandQueue CreateCommandQueue = nullptr;
    };

    IntelExtensionInterfaces mIntelExtensionInterfaces;
};

IntelExtensionImpl::IntelExtensionImpl(const PhysicalDevice& physicalDevice) {
    ASSERT(gpu_info::IsIntel(physicalDevice.GetVendorId()));

    DeviceInfo deviceInfo;

    // Get the display device instance ID
    if (!FillDeviceID(&deviceInfo, physicalDevice)) {
        return;
    }

    // Get the display device instance Handle
    if (!FillDeviceInstanceHandle(&deviceInfo)) {
        return;
    }

    // Get the path of the display device
    if (!FillDeviceDriverPath(&deviceInfo)) {
        return;
    }

    // Load Intel extension DLL
    mIntelExtensionDLLModule = LoadIntelExtensionLibrary(deviceInfo);
    if (mIntelExtensionDLLModule == nullptr) {
        return;
    }

    // Load all the interfaces we need from the Intel extension DLL
    if (!LoadInterfaces()) {
        CleanUp();
        return;
    }

    // Create INTCExtensionContext
    if (!CreateINTCExtensionContext(physicalDevice)) {
        CleanUp();
    }
}

IntelExtensionImpl::~IntelExtensionImpl() {
    CleanUp();
}

void IntelExtensionImpl::CleanUp() {
    if (mIntelExtensionDLLModule != nullptr) {
        if (mINTCExtensionContext != nullptr) {
            ASSERT(mIntelExtensionInterfaces.DestroyDeviceExtensionContext != nullptr);
            mIntelExtensionInterfaces.DestroyDeviceExtensionContext(&mINTCExtensionContext);
        }

        FreeLibrary(mIntelExtensionDLLModule);
        mIntelExtensionDLLModule = nullptr;
        mIntelExtensionInterfaces = {};
    }
}

bool IntelExtensionImpl::LoadInterfaces() {
    ASSERT(mIntelExtensionDLLModule != nullptr);

    // The interfaces required for all extensions
    mIntelExtensionInterfaces.CreateDeviceExtensionContext =
        reinterpret_cast<FN_CreateDeviceExtensionContext>(
            GetProcAddress(mIntelExtensionDLLModule, "_INTC_D3D12_CreateDeviceExtensionContext"));
    if (mIntelExtensionInterfaces.CreateDeviceExtensionContext == nullptr) {
        return false;
    }

    mIntelExtensionInterfaces.DestroyDeviceExtensionContext =
        reinterpret_cast<FN_DestroyDeviceExtensionContext>(
            GetProcAddress(mIntelExtensionDLLModule, "D3D11D3D12DestroyDeviceExtensionContext2"));
    if (mIntelExtensionInterfaces.DestroyDeviceExtensionContext == nullptr) {
        return false;
    }

    mIntelExtensionInterfaces.GetSupportedVersions =
        reinterpret_cast<FN_GetSupportedExtensionVersions>(
            GetProcAddress(mIntelExtensionDLLModule, "D3D12GetSupportedVersions2"));
    if (mIntelExtensionInterfaces.GetSupportedVersions == nullptr) {
        return false;
    }

    // The interface required for Throttle Policy extension. Currently Throttle Policy extension is
    // the only Intel extension we'd like to use in Dawn.
    mIntelExtensionInterfaces.CreateCommandQueue = reinterpret_cast<FN_CreateCommandQueue>(
        GetProcAddress(mIntelExtensionDLLModule, "_INTC_D3D12_CreateCommandQueue"));
    if (mIntelExtensionInterfaces.CreateCommandQueue == nullptr) {
        return false;
    }

    return true;
}

bool IntelExtensionImpl::CreateINTCExtensionContext(const PhysicalDevice& physicalDevice) {
    ASSERT(mIntelExtensionInterfaces.GetSupportedVersions != nullptr);

    ID3D12Device* d3d12Device = physicalDevice.GetDevice().Get();

    // Find out the minimum available extension version that supports Throttle Policy extension.
    constexpr INTCExtensionVersion kThrottlePolicyExtensionMinimumVersion = {1, 0, 0};
    uint32_t supportedExtVersionCount = 0;
    mIntelExtensionInterfaces.GetSupportedVersions(d3d12Device, nullptr, &supportedExtVersionCount);
    if (supportedExtVersionCount == 0) {
        return false;
    }
    std::vector<INTCExtensionVersion> availableVersions(supportedExtVersionCount);
    mIntelExtensionInterfaces.GetSupportedVersions(d3d12Device, availableVersions.data(),
                                                   &supportedExtVersionCount);
    for (INTCExtensionVersion version : availableVersions) {
        // We don't need to compare APIVersion or Revision as both
        // kThrottlePolicyExtensionMinimumVersion.APIVersion and
        // kThrottlePolicyExtensionMinimumVersion.Revision are 0.
        if (version.HWFeatureLevel >= kThrottlePolicyExtensionMinimumVersion.HWFeatureLevel) {
            INTCExtensionInfo intcExtensionInfo = {};
            intcExtensionInfo.RequestedExtensionVersion = version;
            mIntelExtensionInterfaces.CreateDeviceExtensionContext(
                d3d12Device, &mINTCExtensionContext, &intcExtensionInfo, nullptr);
            break;
        }
    }

    return mINTCExtensionContext != nullptr;
}

bool IntelExtensionImpl::IsThrottlePolicyExtensionSupported() const {
    // Currently Intel extension context is created only when Intel Throttle Policy extension is
    // supported.
    return mINTCExtensionContext != nullptr;
}

HRESULT IntelExtensionImpl::CreateCommandQueueWithMaxPerformanceThrottlePolicy(
    D3D12_COMMAND_QUEUE_DESC* d3d12CommandQueueDesc,
    REFIID riid,
    void** ppCommandQueue) {
    ASSERT(mIntelExtensionInterfaces.CreateCommandQueue != nullptr);
    ASSERT(mINTCExtensionContext != nullptr);

    INTC_D3D12_COMMAND_QUEUE_DESC intcDesc = {};
    intcDesc.pD3D12Desc = d3d12CommandQueueDesc;
    intcDesc.CommandThrottlePolicy = INTC_D3D12_COMMAND_QUEUE_THROTTLE_MAX_PERFORMANCE;

    return mIntelExtensionInterfaces.CreateCommandQueue(mINTCExtensionContext, &intcDesc, riid,
                                                        ppCommandQueue);
}

#else
namespace dawn::native::d3d12 {
#endif

// Factory function of IntelExtension
std::unique_ptr<IntelExtension> IntelExtension::Create(const PhysicalDevice& physicalDevice) {
#if DAWN_PLATFORM_IS_WIN32
    std::unique_ptr<IntelExtensionImpl> extension =
        std::make_unique<IntelExtensionImpl>(physicalDevice);

    // Currently we only care about Throttle Policy extension, so we don't need to load the Intel
    // extension DLL if Throttle Policy extension isn't supported.
    if (!extension->IsThrottlePolicyExtensionSupported()) {
        extension = nullptr;
    }
    return std::move(extension);
#else
    return nullptr;
#endif
}

}  // namespace dawn::native::d3d12
