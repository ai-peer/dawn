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

#include "dawn_native/metal/BackendMTL.h"

#include "common/Constants.h"
#include "dawn_native/Instance.h"
#include "dawn_native/MetalBackend.h"
#include "dawn_native/metal/DeviceMTL.h"

#import <IOKit/IOKitLib.h>

#include <map>

namespace dawn_native { namespace metal {

    namespace {
        struct PCIIDs {
            uint32_t vendorId;
            uint32_t deviceId;
        };

        // Map for device name and vendor id.
        // Key: vendor keywords of device name
        // Value: vendor id
        std::map<std::string, uint32_t> kVendorIdMap = {
            {"AMD", kVendorID_AMD},        {"Mali", kVendorID_ARM},
            {"PowerVR", kVendorID_ImgTec}, {"Intel", kVendorID_Intel},
            {"Geforce", kVendorID_Nvidia}, {"Adreno", kVendorID_Qualcomm}};

        // Extracts an integer property from a registry entry.
        uint32_t GetEntryProperty(io_registry_entry_t entry, CFStringRef name) {
            uint32_t value = 0;

            // Recursively search registry entry and its parents for property name
            // The data should release with CFRelease
            CFDataRef data = static_cast<CFDataRef>(IORegistryEntrySearchCFProperty(
                entry, kIOServicePlane, name, kCFAllocatorDefault,
                kIORegistryIterateRecursively | kIORegistryIterateParents));

            if (data == nullptr) {
                return value;
            }

            value = *reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(data));

            CFRelease(data);
            return value;
        }

        // Queries the IO Registry to find the PCI device and vendor IDs of the MTLDevice.
        // The registry entry correponding to [device registryID] doesn't contain the exact PCI ids
        // because it corresponds to a driver. However its parent entry corresponds to the device
        // itself and has uint32_t "device-id" and "registry-id" keys. For example on a dual-GPU
        // MacBook Pro 2017 the IORegistry explorer shows the following tree (simplified here):
        //
        //  - PCI0@0
        //  | - AppleACPIPCI
        //  | | - IGPU@2 (type IOPCIDevice)
        //  | | | - IntelAccelerator (type IOGraphicsAccelerator2)
        //  | | - PEG0@1
        //  | | | - IOPP
        //  | | | | - GFX0@0 (type IOPCIDevice)
        //  | | | | | - AMDRadeonX4000_AMDBaffinGraphicsAccelerator (type IOGraphicsAccelerator2)
        //
        // [device registryID] is the ID for one of the IOGraphicsAccelerator2 and we can see that
        // their parent always is an IOPCIDevice that has properties for the device and vendor IDs.
        MaybeError GetDeviceIORegistryPCIInfo(id<MTLDevice> device, PCIIDs* ids) {
            // Get a matching dictionary for the IOGraphicsAccelerator2
            CFMutableDictionaryRef matchingDict = IORegistryEntryIDMatching([device registryID]);
            if (matchingDict == nullptr) {
                return DAWN_CONTEXT_LOST_ERROR("Failed to create the matching dict for the device");
            }

            // IOServiceGetMatchingService will consume the reference on the matching dictionary,
            // so we don't need to release the dictionary.
            io_registry_entry_t acceleratorEntry =
                IOServiceGetMatchingService(kIOMasterPortDefault, matchingDict);
            if (acceleratorEntry == IO_OBJECT_NULL) {
                return DAWN_CONTEXT_LOST_ERROR(
                    "Failed to get the IO registry entry for the accelerator");
            }

            // Get the parent entry that will be the IOPCIDevice
            io_registry_entry_t deviceEntry = IO_OBJECT_NULL;
            if (IORegistryEntryGetParentEntry(acceleratorEntry, kIOServicePlane, &deviceEntry) !=
                kIOReturnSuccess) {
                IOObjectRelease(acceleratorEntry);
                return DAWN_CONTEXT_LOST_ERROR(
                    "Failed to get the IO registry entry for the device");
            }

            ASSERT(deviceEntry != IO_OBJECT_NULL);
            IOObjectRelease(acceleratorEntry);

            uint32_t vendorId = GetEntryProperty(deviceEntry, CFSTR("vendor-id"));
            uint32_t deviceId = GetEntryProperty(deviceEntry, CFSTR("device-id"));

            *ids = PCIIDs{vendorId, deviceId};

            IOObjectRelease(deviceEntry);

            return {};
        }

        // Find vendor id from vendor name
        uint32_t GetVendorIdFromVendors(id<MTLDevice> device) {
            uint32_t vendorId = 0;

            std::string deviceName = std::string([device.name UTF8String]);

            std::map<std::string, uint32_t>::const_iterator it = kVendorIdMap.begin();
            while (it != kVendorIdMap.end()) {
                // Matching device name with vendor name
                if (deviceName.find(it->first) != std::string::npos) {
                    vendorId = it->second;
                    break;
                }
                it++;
            }
            return vendorId;
        }

        // Get PCI device ID and vendor ID of IOPCIDevice and match them with the name of MTLDevice.
        MaybeError GetDeviceIOPCIInfo(id<MTLDevice> device, PCIIDs* ids) {
            uint32_t vendorId = GetVendorIdFromVendors(device);
            if (vendorId == 0) {
                return DAWN_CONTEXT_LOST_ERROR("Failed to find vendor id with the device");
            }

            // matchingDict will be consumed by IOServiceGetMatchingServices, no need to release it.
            CFMutableDictionaryRef matchingDict = IOServiceMatching("IOPCIDevice");
            if (matchingDict == nullptr) {
                return DAWN_CONTEXT_LOST_ERROR("Failed to create the matching dict for the device");
            }

            io_iterator_t entryIterator;
            if (IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, &entryIterator) !=
                kIOReturnSuccess) {
                return DAWN_CONTEXT_LOST_ERROR("Failed to get the IO service entry for the device");
            }

            uint32_t deviceId = 0;
            io_registry_entry_t entry = IO_OBJECT_NULL;
            while ((entry = IOIteratorNext(entryIterator)) != IO_OBJECT_NULL) {
                constexpr uint32_t kClassCodeDisplayVGA = 0x30000;
                uint32_t classCode = GetEntryProperty(entry, CFSTR("class-code"));
                uint32_t vendorIdt = GetEntryProperty(entry, CFSTR("vendor-id"));

                if (classCode == kClassCodeDisplayVGA && vendorIdt == vendorId) {
                    deviceId = GetEntryProperty(entry, CFSTR("device-id"));
                    break;
                }
                IOObjectRelease(entry);
            }
            IOObjectRelease(entryIterator);

            *ids = PCIIDs{vendorId, deviceId};
            return {};
        }

        MaybeError GetDevicePCIInfo(id<MTLDevice> device, PCIIDs* ids) {
            // [device registryID] is introduced on macOS 10.13+, otherwise workaround to get vendor
            // id by vendor name on old macOS
            if ([NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:{10, 13, 0}]) {
                return GetDeviceIORegistryPCIInfo(device, ids);
            } else {
                return GetDeviceIOPCIInfo(device, ids);
            }
        }

        bool IsMetalSupported() {
            // Metal was first introduced in macOS 10.11
            NSOperatingSystemVersion macOS10_11 = {10, 11, 0};
            return [NSProcessInfo.processInfo isOperatingSystemAtLeastVersion:macOS10_11];
        }
    }  // anonymous namespace

    // The Metal backend's Adapter.

    class Adapter : public AdapterBase {
      public:
        Adapter(InstanceBase* instance, id<MTLDevice> device)
            : AdapterBase(instance, BackendType::Metal), mDevice([device retain]) {
            mPCIInfo.name = std::string([mDevice.name UTF8String]);

            PCIIDs ids;
            if (!instance->ConsumedError(GetDevicePCIInfo(device, &ids))) {
                mPCIInfo.vendorId = ids.vendorId;
                mPCIInfo.deviceId = ids.deviceId;
            };

            if ([device isLowPower]) {
                mDeviceType = DeviceType::IntegratedGPU;
            } else {
                mDeviceType = DeviceType::DiscreteGPU;
            }
        }

        ~Adapter() override {
            [mDevice release];
        }

      private:
        ResultOrError<DeviceBase*> CreateDeviceImpl(const DeviceDescriptor* descriptor) override {
            return {new Device(this, mDevice, descriptor)};
        }

        id<MTLDevice> mDevice = nil;
    };

    // Implementation of the Metal backend's BackendConnection

    Backend::Backend(InstanceBase* instance) : BackendConnection(instance, BackendType::Metal) {
        if (GetInstance()->IsBackendValidationEnabled()) {
            setenv("METAL_DEVICE_WRAPPER_TYPE", "1", 1);
        }
    }

    std::vector<std::unique_ptr<AdapterBase>> Backend::DiscoverDefaultAdapters() {
        NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();

        std::vector<std::unique_ptr<AdapterBase>> adapters;
        for (id<MTLDevice> device in devices) {
            adapters.push_back(std::make_unique<Adapter>(GetInstance(), device));
        }

        [devices release];
        return adapters;
    }

    BackendConnection* Connect(InstanceBase* instance) {
        if (!IsMetalSupported()) {
            return nullptr;
        }
        return new Backend(instance);
    }

}}  // namespace dawn_native::metal
