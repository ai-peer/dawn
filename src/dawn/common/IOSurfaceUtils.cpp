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

#include "dawn/common/IOSurfaceUtils.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CVPixelBuffer.h>

#include "dawn/common/Assert.h"

namespace dawn {

namespace {

void AddIntegerValue(CFMutableDictionaryRef dictionary, const CFStringRef key, int32_t value) {
    CFNumberRef number(CFNumberCreate(nullptr, kCFNumberSInt32Type, &value));
    CFDictionaryAddValue(dictionary, key, number);
    CFRelease(number);
}

OSType ToCVFormat(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
            return kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        case wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm:
            return kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange;
        default:
            DAWN_UNREACHABLE();
            return 0;
    }
}

uint32_t NumPlanes(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
        case wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm:
            return 2;
        default:
            DAWN_UNREACHABLE();
            return 1;
    }
}

size_t GetSubSamplingFactorPerPlane(wgpu::TextureFormat format, size_t plane) {
    switch (format) {
        case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
        case wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm:
            return plane == 0 ? 1 : 2;
        default:
            DAWN_UNREACHABLE();
            return 0;
    }
}

size_t BytesPerElement(wgpu::TextureFormat format, size_t plane) {
    switch (format) {
        case wgpu::TextureFormat::R8BG8Biplanar420Unorm:
            return plane == 0 ? 1 : 2;
        case wgpu::TextureFormat::R10X6BG10X6Biplanar420Unorm:
            return plane == 0 ? 2 : 4;
        default:
            DAWN_UNREACHABLE();
            return 0;
    }
}

}  // namespace

IOSurfaceRef CreateMultiPlanarIOSurface(wgpu::TextureFormat format,
                                        uint32_t width,
                                        uint32_t height) {
    CFMutableDictionaryRef dict(CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    AddIntegerValue(dict, kIOSurfaceWidth, width);
    AddIntegerValue(dict, kIOSurfaceHeight, height);
    AddIntegerValue(dict, kIOSurfacePixelFormat, ToCVFormat(format));

    size_t numPlanes = NumPlanes(format);

    CFMutableArrayRef planes(
        CFArrayCreateMutable(kCFAllocatorDefault, numPlanes, &kCFTypeArrayCallBacks));
    size_t totalBytesAlloc = 0;
    for (size_t plane = 0; plane < numPlanes; ++plane) {
        const size_t factor = GetSubSamplingFactorPerPlane(format, plane);
        const size_t planeWidth = width / factor;
        const size_t planeHeight = height / factor;
        const size_t planeBytesPerElement = BytesPerElement(format, plane);
        const size_t planeBytesPerRow =
            IOSurfaceAlignProperty(kIOSurfacePlaneBytesPerRow, planeWidth * planeBytesPerElement);
        const size_t planeBytesAlloc =
            IOSurfaceAlignProperty(kIOSurfacePlaneSize, planeHeight * planeBytesPerRow);
        const size_t planeOffset = IOSurfaceAlignProperty(kIOSurfacePlaneOffset, totalBytesAlloc);

        CFMutableDictionaryRef planeInfo(
            CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks));

        AddIntegerValue(planeInfo, kIOSurfacePlaneWidth, planeWidth);
        AddIntegerValue(planeInfo, kIOSurfacePlaneHeight, planeHeight);
        AddIntegerValue(planeInfo, kIOSurfacePlaneBytesPerElement, planeBytesPerElement);
        AddIntegerValue(planeInfo, kIOSurfacePlaneBytesPerRow, planeBytesPerRow);
        AddIntegerValue(planeInfo, kIOSurfacePlaneSize, planeBytesAlloc);
        AddIntegerValue(planeInfo, kIOSurfacePlaneOffset, planeOffset);
        CFArrayAppendValue(planes, planeInfo);
        CFRelease(planeInfo);
        totalBytesAlloc = planeOffset + planeBytesAlloc;
    }
    CFDictionaryAddValue(dict, kIOSurfacePlaneInfo, planes);
    CFRelease(planes);

    totalBytesAlloc = IOSurfaceAlignProperty(kIOSurfaceAllocSize, totalBytesAlloc);
    AddIntegerValue(dict, kIOSurfaceAllocSize, totalBytesAlloc);

    IOSurfaceRef surface = IOSurfaceCreate(dict);
    CFRelease(dict);

    return surface;
}
}  // namespace dawn
