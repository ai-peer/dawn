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

#include "common/Assert.h"
#include "dawn_native/WorkerThread.h"

namespace dawn_platform {

    CachingInterface::CachingInterface() = default;

    CachingInterface::~CachingInterface() = default;

    WorkerTaskPool::~WorkerTaskPool() {
    }

    void WorkerTaskPool::PostWorkerTask(std::function<void(void*)> callback, void* userdata) {
        callback(userdata);
    }

    void WorkerTaskPool::TaskFinished() {
    }

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

    dawn_platform::WorkerTaskPool* Platform::CreateWorkerTaskPool() {
        return nullptr;
    }

}  // namespace dawn_platform
