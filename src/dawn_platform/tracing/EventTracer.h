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

#ifndef DAWN_TRACING_EVENT_TRACER_H_
#define DAWN_TRACING_EVENT_TRACER_H_

#include <stdint.h>

namespace dawn_platform { namespace tracing {

    using TraceEventHandle = uint64_t;

    const unsigned char* GetTraceCategoryEnabledFlag(const char* name);
    TraceEventHandle AddTraceEvent(char phase,
                                   const unsigned char* categoryGroupEnabled,
                                   const char* name,
                                   unsigned long long id,
                                   int numArgs,
                                   const char** argNames,
                                   const unsigned char* argTypes,
                                   const unsigned long long* argValues,
                                   unsigned char flags);

}}  // namespace dawn_platform::tracing

#endif  // DAWN_TRACING_EVENT_TRACER_H_
