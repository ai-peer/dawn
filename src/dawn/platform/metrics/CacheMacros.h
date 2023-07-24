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

// This header provides macros for adding Chromium UMA histogram stats for disk caching.

#ifndef SRC_DAWN_PLATFORM_METRICS_CACHE_MACROS_H_
#define SRC_DAWN_PLATFORM_METRICS_CACHE_MACROS_H_

#include <utility>

#include "dawn/platform/metrics/HistogramMacros.h"

// Scoped class which logs time for a cache hit.
#define SCOPED_DAWN_CACHE_HIT_TIMER(platform, name) \
    SCOPED_DAWN_HISTOGRAM_TIMER(platform, "CacheHit." name)

// Scoped class which logs time for a cache miss.
#define SCOPED_DAWN_CACHE_MISS_TIMER(platform, name) \
    SCOPED_DAWN_HISTOGRAM_TIMER(platform, "CacheMiss." name)

// Wrapper for common use cases of DAWN_TRY_LOAD_OR_RUN or LoadOrRun with CacheRequests that
// captures the platform and returns an appropriate lambda with a scoped timer.
#define SCOPED_DAWN_CACHE_HIT_FROM_BLOB(platform, name, fromBlobFn) \
    [p = platform](Blob b) {                                        \
        SCOPED_DAWN_CACHE_HIT_TIMER(p, name);                       \
        return fromBlobFn(std::move(b));                            \
    }

#endif  // SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_MACROS_H_
