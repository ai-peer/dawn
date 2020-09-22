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

#include "dawn_native/metal/QuerySetMTL.h"

#include "common/Math.h"
#include "common/Platform.h"
#include "dawn_native/metal/DeviceMTL.h"

namespace dawn_native { namespace metal {

    namespace {

        MTLCounterSampleBufferDescriptor* CreateCounterSampleBufferDescriptor(
            id<MTLDevice> device,
            MTLCommonCounterSet counterSet,
            uint32_t count) API_AVAILABLE(macos(10.15)) {
            MTLCounterSampleBufferDescriptor* descriptor = [MTLCounterSampleBufferDescriptor new];
            // The counterSet in  MTLCounterSampleBufferDescriptor specifies which set of counters
            // is sampled. We need to iterate through the deivce's counterSets to determine which
            // counter sets the device supports, then select the counterSet which name is same with
            // the counter set in MTLCommonCounterSet.
            for (id<MTLCounterSet> set in device.counterSets) {
                if ([set.name isEqualToString:counterSet]) {
                    descriptor.counterSet = set;
                    break;
                }
            }
            ASSERT(descriptor.counterSet != nil);
            descriptor.sampleCount = count;
            // TODO(hao.x.li): The storage mode should be MTLStorageModePrivate due to the sample
            // buffer is only accessed by GPU, but it crash on Intel GPUs, so here use
            // MTLStorageModeShared instead, need further investigate the root cause.
            descriptor.storageMode = MTLStorageModeShared;

            return descriptor;
        }
    }

    // static
    ResultOrError<QuerySet*> QuerySet::Create(Device* device,
                                              const QuerySetDescriptor* descriptor) {
        Ref<QuerySet> queryset = AcquireRef(new QuerySet(device, descriptor));
        DAWN_TRY(queryset->Initialize());
        return queryset.Detach();
    }

    MaybeError QuerySet::Initialize() {
        id<MTLDevice> mtlDevice = ToBackend(GetDevice())->GetMTLDevice();

        switch (GetQueryType()) {
            case wgpu::QueryType::Occlusion: {
                // Create buffer for writing 64-bit results.
                uint64_t bufferSize = static_cast<uint64_t>(GetQueryCount()) * sizeof(uint64_t);
                mVisibilityBuffer = [mtlDevice newBufferWithLength:bufferSize
                                                           options:MTLResourceStorageModePrivate];
                break;
            }
            case wgpu::QueryType::PipelineStatistics:
#if defined(DAWN_PLATFORM_MACOS)
                if (@available(macOS 10.15, *)) {
                    MTLCounterSampleBufferDescriptor* descriptor =
                        CreateCounterSampleBufferDescriptor(mtlDevice, MTLCommonCounterSetStatistic,
                                                            GetQueryCount());

                    NSError* error = nil;
                    mCounterSampleBuffer =
                        [mtlDevice newCounterSampleBufferWithDescriptor:descriptor error:&error];
                    if (error != nil) {
                        NSLog(@" error => %@", error);
                        return DAWN_INTERNAL_ERROR("Error creating query set");
                    }
                }
#else
                UNREACHABLE();
#endif
                break;
            case wgpu::QueryType::Timestamp:
#if defined(DAWN_PLATFORM_MACOS)
                if (@available(macOS 10.15, *)) {
                    MTLCounterSampleBufferDescriptor* descriptor =
                        CreateCounterSampleBufferDescriptor(mtlDevice, MTLCommonCounterSetTimestamp,
                                                            GetQueryCount());

                    NSError* error = nil;
                    mCounterSampleBuffer =
                        [mtlDevice newCounterSampleBufferWithDescriptor:descriptor error:&error];
                    if (error != nil) {
                        NSLog(@" error => %@", error);
                        return DAWN_INTERNAL_ERROR("Error creating query set");
                    }
                }
#else
                UNREACHABLE();
#endif
                break;
            default:
                UNREACHABLE();
                break;
        }

        return {};
    }

    id<MTLBuffer> QuerySet::GetVisibilityBuffer() const {
        return mVisibilityBuffer;
    }

    id<MTLCounterSampleBuffer> QuerySet::GetCounterSampleBuffer() const
        API_AVAILABLE(macos(10.15)) {
        return mCounterSampleBuffer;
    }

    QuerySet::~QuerySet() {
        DestroyInternal();
    }

    void QuerySet::DestroyImpl() {
        if (mVisibilityBuffer != nil) {
            [mVisibilityBuffer release];
            mVisibilityBuffer = nil;
        }

#if defined(DAWN_PLATFORM_MACOS)
        if (@available(macOS 10.15, *)) {
            if (mCounterSampleBuffer != nil) {
                [mCounterSampleBuffer release];
                mCounterSampleBuffer = nil;
            }
        }
#endif
    }

}}  // namespace dawn_native::metal
