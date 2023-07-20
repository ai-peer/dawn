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

#include "dawn/tests/white_box/SharedTextureMemoryTests.h"
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
    std::string Name() const override { return "IOSurface"; }

    std::vector<wgpu::FeatureName> RequiredFeatures() const override {
        return {wgpu::FeatureName::SharedTextureMemoryIOSurface,
                wgpu::FeatureName::SharedFenceMTLSharedEvent,
                wgpu::FeatureName::DawnMultiPlanarFormats};
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

DAWN_INSTANTIATE_PREFIXED_TEST_P(Metal,
                                 SharedTextureMemoryTests,
                                 {MetalBackend()},
                                 {new Backend()});

DAWN_INSTANTIATE_PREFIXED_TEST_P(Metal,
                                 SharedTextureMemoryNoFeatureTests,
                                 {MetalBackend()},
                                 {new Backend()});

}  // anonymous namespace
}  // namespace dawn
