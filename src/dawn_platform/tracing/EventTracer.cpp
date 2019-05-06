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

#include "dawn_platform/tracing/EventTracer.h"
#include "common/Assert.h"
#include "dawn_platform/DawnPlatform.h"

namespace dawn_platform { namespace tracing {

    const unsigned char* GetTraceCategoryEnabledFlag(const char* name) {
        Platform* platform = Platform::Get();

        static unsigned char disabled = 0;
        if (platform == nullptr) {
            return &disabled;
        }

        const unsigned char* categoryEnabledFlag = platform->GetTraceCategoryEnabledFlag(name);
        if (categoryEnabledFlag != nullptr) {
            return categoryEnabledFlag;
        }

        return &disabled;
    }

    TraceEventHandle AddTraceEvent(char phase,
                                   const unsigned char* categoryGroupEnabled,
                                   const char* name,
                                   unsigned long long id,
                                   int numArgs,
                                   const char** argNames,
                                   const unsigned char* argTypes,
                                   const unsigned long long* argValues,
                                   unsigned char flags) {
        Platform* platform = Platform::Get();
        ASSERT(platform != nullptr);

        double timestamp = platform->MonotonicallyIncreasingTime();
        if (timestamp != 0) {
            TraceEventHandle handle =
                platform->AddTraceEvent(phase, categoryGroupEnabled, name, id, timestamp, numArgs,
                                        argNames, argTypes, argValues, flags);
            return handle;
        }

        return static_cast<TraceEventHandle>(0);
    }

}}  // namespace dawn_platform::tracing
