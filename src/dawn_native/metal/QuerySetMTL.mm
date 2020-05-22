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
#include "dawn_native/metal/DeviceMTL.h"

namespace dawn_native { namespace metal {

    namespace {
        // The size of uniform buffer and storage buffer need to be aligned to 16 bytes which is the
        // largest alignment of supported data types
        static constexpr uint32_t kMinUniformOrStorageBufferAlignment = 16u;

        id<MTLCounterSampleBuffer> CreateCounterSampleBuffer(id<MTLDevice> device,
                                                             MTLCommonCounterSet counterSet,
                                                             uint32_t count)
            API_AVAILABLE(macos(10.15)) {
            MTLCounterSampleBufferDescriptor* descriptor = [MTLCounterSampleBufferDescriptor new];
            for (id<MTLCounterSet> set in device.counterSets) {
                if ([set.name isEqualToString:counterSet]) {
                    descriptor.counterSet = set;
                    break;
                }
            }
            ASSERT(descriptor.counterSet != nil);
            descriptor.sampleCount = count;
            descriptor.storageMode = MTLStorageModeShared;
            NSError* error = nil;
            id<MTLCounterSampleBuffer> counterSampleBuffer =
                [device newCounterSampleBufferWithDescriptor:descriptor error:&error];
            ASSERT(error == nil);
            return counterSampleBuffer;
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
        id<MTLDevice> device = ToBackend(GetDevice())->GetMTLDevice();

        switch (GetQueryType()) {
            case wgpu::QueryType::Occlusion: {
                // Create buffer for writing 64-bit results of Occlusion query
                uint64_t bufferSize = GetQueryCount() * sizeof(uint64_t);
                if (bufferSize >
                    std::numeric_limits<uint64_t>::max() - kMinUniformOrStorageBufferAlignment) {
                    return DAWN_OUT_OF_MEMORY_ERROR("QuerySet count is too large");
                }
                bufferSize = Align(bufferSize, kMinUniformOrStorageBufferAlignment);
                mVisibilityBuffer = [device newBufferWithLength:bufferSize
                                                        options:MTLResourceStorageModeShared];
                break;
            }
            case wgpu::QueryType::PipelineStatistics:
                if (@available(macOS 10.15, *)) {
                    mCounterSampleBuffer = CreateCounterSampleBuffer(
                        device, MTLCommonCounterSetStatistic, GetQueryCount());
                }
                break;
            case wgpu::QueryType::Timestamp:
                if (@available(macOS 10.15, *)) {
                    mCounterSampleBuffer = CreateCounterSampleBuffer(
                        device, MTLCommonCounterSetTimestamp, GetQueryCount());
                }
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

        if (@available(macOS 10.15, *)) {
            if (mCounterSampleBuffer != nil) {
                [mCounterSampleBuffer release];
                mCounterSampleBuffer = nil;
            }
        }
    }

}}  // namespace dawn_native::metal
