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

#include "dawn_native/Instance.h"
#include "dawn_native/MetalBackend.h"
#include "dawn_native/metal/DeviceMTL.h"

#include <IOKit/IOKitLib.h>

namespace dawn_native { namespace metal {

    namespace {

        struct PCIIDs {
            uint32_t vendorId;
            uint32_t deviceId;
        };

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

            // Get the dictionary of properties for the IOPCIDevice
            CFMutableDictionaryRef deviceProperties;
            if (IORegistryEntryCreateCFProperties(deviceEntry, &deviceProperties,
                                                  kCFAllocatorDefault,
                                                  kNilOptions) != kIOReturnSuccess) {
                IOObjectRelease(deviceEntry);
                return DAWN_CONTEXT_LOST_ERROR(
                    "Failed to get the IO registry properties for the device");
            }

            ASSERT(deviceProperties != nullptr);
            IOObjectRelease(deviceEntry);

            // Extract the device and vendor ID from the properties. The refs don't need to be
            // released because they follow the "Get" rule.
            CFDataRef vendorIdRef, deviceIdRef;
            Boolean success;
            success = CFDictionaryGetValueIfPresent(deviceProperties, CFSTR("vendor-id"),
                                                    (const void**)&vendorIdRef);
            success &= CFDictionaryGetValueIfPresent(deviceProperties, CFSTR("device-id"),
                                                     (const void**)&deviceIdRef);

            if (!success) {
                CFRelease(deviceProperties);
                return DAWN_CONTEXT_LOST_ERROR("Failed to extract vendor and device ID.");
            }

            uint32_t vendorId = *reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(vendorIdRef));
            uint32_t deviceId = *reinterpret_cast<const uint32_t*>(CFDataGetBytePtr(deviceIdRef));

            CFRelease(deviceProperties);

            *ids = PCIIDs{vendorId, deviceId};
            return {};
        }

    }  // anonymous namespace

    // The Metal backend's Adapter.

    class Adapter : public AdapterBase {
      public:
        Adapter(InstanceBase* instance, id<MTLDevice> device)
            : AdapterBase(instance, BackendType::Metal), mDevice([device retain]) {
            mPCIInfo.name = std::string([mDevice.name UTF8String]);

            PCIIDs ids;
            if (!instance->ConsumedError(GetDeviceIORegistryPCIInfo(device, &ids))) {
                mPCIInfo.vendorId = ids.vendorId;
                mPCIInfo.deviceId = ids.deviceId;
            }
        }

        ~Adapter() override {
            [mDevice release];
        }

      private:
        ResultOrError<DeviceBase*> CreateDeviceImpl() override {
            return {new Device(this, mDevice)};
        }

        id<MTLDevice> mDevice = nil;
    };

    // Implementation of the Metal backend's BackendConnection

    Backend::Backend(InstanceBase* instance) : BackendConnection(instance, BackendType::Metal) {
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
        return new Backend(instance);
    }

}}  // namespace dawn_native::metal
