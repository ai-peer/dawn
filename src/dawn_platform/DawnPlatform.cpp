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

#include "dawn_platform/DawnPlatform.h"
#include "dawn_platform/CachedBlob.h"
#include "dawn_platform/WorkerThread.h"

#include "common/Assert.h"

namespace dawn_platform {

    CachingInterface::CachingInterface() = default;

    CachingInterface::~CachingInterface() = default;

    Platform::Platform() = default;

    Platform::~Platform() = default;

    const unsigned char* Platform::GetTraceCategoryEnabledFlag(TraceCategory category) {
        static unsigned char disabled = 0;
        return &disabled;
    }

    double Platform::MonotonicallyIncreasingTime() {
        return 0;
    }

    uint64_t Platform::AddTraceEvent(char phase,
                                     const unsigned char* categoryGroupEnabled,
                                     const char* name,
                                     uint64_t id,
                                     double timestamp,
                                     int numArgs,
                                     const char** argNames,
                                     const unsigned char* argTypes,
                                     const uint64_t* argValues,
                                     unsigned char flags) {
        // AddTraceEvent cannot be called if events are disabled.
        ASSERT(false);
        return 0;
    }

    dawn_platform::CachingInterface* Platform::GetCachingInterface(const void* fingerprint,
                                                                   size_t fingerprintSize) {
        return nullptr;
    }

    std::unique_ptr<dawn_platform::WorkerTaskPool> Platform::CreateWorkerTaskPool() {
        return std::make_unique<AsyncWorkerThreadPool>();
    }

    ScopedCachedData Platform::CreateCachedData(const uint8_t* data, size_t size) const {
        return ScopedCachedData(new CachedBlob(data, size));
    }

    ScopedCachedData::ScopedCachedData(CachedData* blob) : mBlob(blob) {
    }

    ScopedCachedData::ScopedCachedData(const ScopedCachedData& other) {
        ReferenceCachedData(other.mBlob);
        ReleaseCachedData(mBlob);
        mBlob = other.mBlob;
    }

    ScopedCachedData::~ScopedCachedData() {
        ReleaseCachedData(mBlob);
    }

    bool ScopedCachedData::operator!=(const ScopedCachedData& other) const {
        return mBlob != other.mBlob;
    }

    bool ScopedCachedData::operator==(const ScopedCachedData& other) const {
        return mBlob == other.mBlob;
    }

    ScopedCachedData& ScopedCachedData::operator=(const ScopedCachedData& other) {
        if (&other != this) {
            ReferenceCachedData(other.mBlob);
            ReleaseCachedData(mBlob);
            mBlob = other.mBlob;
        }
        return *this;
    }

    CachedData* ScopedCachedData::operator->() const {
        return mBlob;
    }

    CachedData* ScopedCachedData::Get() const {
        return mBlob;
    }

    void ScopedCachedData::ReferenceCachedData(CachedData* blob) const {
        if (blob != nullptr) {
            blob->Reference();
        }
    }

    void ScopedCachedData::ReleaseCachedData(CachedData* blob) const {
        if (blob != nullptr) {
            blob->Release();
        }
    }

}  // namespace dawn_platform
