// Copyright 2023 The Dawn Authors
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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <IOSurface/IOSurface.h>

#import <Metal/Metal.h>

#include "dawn/tests/end2end/SharedTextureMemoryTests.h"
#include "dawn/webgpu_cpp.h"

namespace dawn {
namespace {

void AddIntegerValue(CFMutableDictionaryRef dictionary, const CFStringRef key, int32_t value) {
    CFNumberRef number = CFNumberCreate(nullptr, kCFNumberSInt32Type, &value);
    CFDictionaryAddValue(dictionary, key, number);
    CFRelease(number);
}

class Backend : public SharedTextureMemoryTestBackend {
  public:
    static Backend* GetInstance() {
        static Backend* b = new Backend();
        return b;
    }

    std::string Name() const override { return "IOSurface"; }

    std::vector<wgpu::FeatureName> RequiredFeatures() const override {
        return {wgpu::FeatureName::SharedTextureMemoryIOSurface,
                wgpu::FeatureName::SharedFenceMTLSharedEvent,
                wgpu::FeatureName::DawnMultiPlanarFormats};
    }

    // Create one basic shared texture memory. It should support most operations.
    wgpu::SharedTextureMemory CreateSharedTextureMemory(wgpu::Device& device) override {
        CFMutableDictionaryRef dict =
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        AddIntegerValue(dict, kIOSurfaceWidth, 16);
        AddIntegerValue(dict, kIOSurfaceHeight, 16);
        AddIntegerValue(dict, kIOSurfacePixelFormat, kCVPixelFormatType_32RGBA);
        AddIntegerValue(dict, kIOSurfaceBytesPerElement, 4);

        wgpu::SharedTextureMemoryIOSurfaceDescriptor ioSurfaceDesc;
        ioSurfaceDesc.ioSurface = IOSurfaceCreate(dict);
        CFRelease(dict);

        wgpu::SharedTextureMemoryDescriptor desc;
        desc.nextInChain = &ioSurfaceDesc;

        return device.ImportSharedTextureMemory(&desc);
    }

    std::vector<std::vector<wgpu::SharedTextureMemory>> CreatePerDeviceSharedTextureMemories(
        const std::vector<wgpu::Device>& devices) override {
        std::vector<std::vector<wgpu::SharedTextureMemory>> memories;
        for (auto [format, bytesPerElement] : {
                 std::make_pair(kCVPixelFormatType_64RGBAHalf, 8),
                 std::make_pair(kCVPixelFormatType_TwoComponent16Half, 4),
                 std::make_pair(kCVPixelFormatType_OneComponent16Half, 2),
                 std::make_pair(kCVPixelFormatType_ARGB2101010LEPacked, 4),
                 std::make_pair(kCVPixelFormatType_32RGBA, 4),
                 std::make_pair(kCVPixelFormatType_32BGRA, 4),
                 std::make_pair(kCVPixelFormatType_TwoComponent8, 2),
                 std::make_pair(kCVPixelFormatType_OneComponent8, 1),
                 std::make_pair(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange, 4),
             }) {
            for (uint32_t size : {4, 64}) {
                CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
                    kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                    &kCFTypeDictionaryValueCallBacks);
                AddIntegerValue(dict, kIOSurfaceWidth, size);
                AddIntegerValue(dict, kIOSurfaceHeight, size);
                AddIntegerValue(dict, kIOSurfacePixelFormat, format);
                AddIntegerValue(dict, kIOSurfaceBytesPerElement, bytesPerElement);

                wgpu::SharedTextureMemoryIOSurfaceDescriptor ioSurfaceDesc;
                ioSurfaceDesc.ioSurface = IOSurfaceCreate(dict);
                CFRelease(dict);

                wgpu::SharedTextureMemoryDescriptor desc;
                desc.nextInChain = &ioSurfaceDesc;

                std::vector<wgpu::SharedTextureMemory> perDeviceMemories;
                for (auto& device : devices) {
                    perDeviceMemories.push_back(device.ImportSharedTextureMemory(&desc));
                }
                memories.push_back(std::move(perDeviceMemories));
            }
        }

        return memories;
    }

    wgpu::SharedFence ImportFenceTo(const wgpu::Device& importingDevice,
                                    const wgpu::SharedFence& fence) override {
        wgpu::SharedFenceMTLSharedEventExportInfo sharedEventInfo;
        wgpu::SharedFenceExportInfo exportInfo;
        exportInfo.nextInChain = &sharedEventInfo;

        fence.ExportInfo(&exportInfo);

        wgpu::SharedFenceMTLSharedEventDescriptor sharedEventDesc;
        sharedEventDesc.sharedEvent = sharedEventInfo.sharedEvent;

        wgpu::SharedFenceDescriptor fenceDesc;
        fenceDesc.nextInChain = &sharedEventDesc;

        return importingDevice.ImportSharedFence(&fenceDesc);
    }
};

// Test that a shared event can be imported, and then exported.
TEST_P(SharedTextureMemoryTests, SharedFenceSuccessfulImportExport) {
    if (@available(macOS 10.14, iOS 12.0, *)) {
        id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
        id<MTLSharedEvent> sharedEvent = [mtlDevice newSharedEvent];

        wgpu::SharedFenceMTLSharedEventDescriptor sharedEventDesc;
        sharedEventDesc.sharedEvent = static_cast<void*>(sharedEvent);

        wgpu::SharedFenceDescriptor fenceDesc;
        fenceDesc.nextInChain = &sharedEventDesc;

        wgpu::SharedFence fence = device.ImportSharedFence(&fenceDesc);

        // Release the Metal objects. They should be retained by the implementation.
        [sharedEvent release];
        [mtlDevice release];

        wgpu::SharedFenceMTLSharedEventExportInfo sharedEventInfo;
        wgpu::SharedFenceExportInfo exportInfo;
        exportInfo.nextInChain = &sharedEventInfo;
        fence.ExportInfo(&exportInfo);

        // The exported event should be the same as the imported one.
        EXPECT_EQ(sharedEventInfo.sharedEvent, sharedEventDesc.sharedEvent);
        EXPECT_EQ(exportInfo.type, wgpu::SharedFenceType::MTLSharedEvent);
    }
}

// Test exporting info from a shared fence with no chained struct.
// It should be valid and the fence type is exported.
TEST_P(SharedTextureMemoryTests, SharedFenceExportInfoNoChainedStruct) {
    if (@available(macOS 10.14, iOS 12.0, *)) {
        id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
        id<MTLSharedEvent> sharedEvent = [mtlDevice newSharedEvent];

        wgpu::SharedFenceMTLSharedEventDescriptor sharedEventDesc;
        sharedEventDesc.sharedEvent = static_cast<void*>(sharedEvent);

        wgpu::SharedFenceDescriptor fenceDesc;
        fenceDesc.nextInChain = &sharedEventDesc;

        wgpu::SharedFence fence = device.ImportSharedFence(&fenceDesc);

        // Release the Metal objects. They should be retained by the implementation.
        [sharedEvent release];
        [mtlDevice release];

        // Test no chained struct.
        wgpu::SharedFenceExportInfo exportInfo;
        exportInfo.nextInChain = nullptr;

        fence.ExportInfo(&exportInfo);
        EXPECT_EQ(exportInfo.type, wgpu::SharedFenceType::MTLSharedEvent);
    }
}

// Test exporting info from a shared fence with an invalid chained struct.
// It should not be valid, but the fence type should still be exported.
TEST_P(SharedTextureMemoryTests, SharedFenceExportInfoInvalidChainedStruct) {
    if (@available(macOS 10.14, iOS 12.0, *)) {
        id<MTLDevice> mtlDevice = MTLCreateSystemDefaultDevice();
        id<MTLSharedEvent> sharedEvent = [mtlDevice newSharedEvent];

        wgpu::SharedFenceMTLSharedEventDescriptor sharedEventDesc;
        sharedEventDesc.sharedEvent = static_cast<void*>(sharedEvent);

        wgpu::SharedFenceDescriptor fenceDesc;
        fenceDesc.nextInChain = &sharedEventDesc;

        wgpu::SharedFence fence = device.ImportSharedFence(&fenceDesc);

        // Release the Metal objects. They should be retained by the implementation.
        [sharedEvent release];
        [mtlDevice release];

        wgpu::SharedFenceVkSemaphoreOpaqueFDExportInfo vkInfo;
        wgpu::SharedFenceExportInfo exportInfo;
        exportInfo.nextInChain = &vkInfo;

        ASSERT_DEVICE_ERROR(fence.ExportInfo(&exportInfo));
        EXPECT_EQ(exportInfo.type, wgpu::SharedFenceType::MTLSharedEvent);
    }
}

DAWN_INSTANTIATE_PREFIXED_TEST_P(Metal,
                                 SharedTextureMemoryNoFeatureTests,
                                 {MetalBackend()},
                                 {Backend::GetInstance()});

DAWN_INSTANTIATE_PREFIXED_TEST_P(Metal,
                                 SharedTextureMemoryTests,
                                 {MetalBackend()},
                                 {Backend::GetInstance()});

}  // anonymous namespace
}  // namespace dawn
