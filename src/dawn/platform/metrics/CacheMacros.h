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

// Short cache timings - up to 10 seconds.
#define DAWN_CACHE_HIT_HISTOGRAM_TIMES(platform, name, sample_ms) \
    DAWN_HISTOGRAM_TIMES(platform, "CacheHit." name, sample_ms)

#define DAWN_CACHE_MISS_HISTOGRAM_TIMES(platform, name, sample_ms) \
    DAWN_HISTOGRAM_TIMES(platform, "CacheMiss." name, sample_ms)

// Scoped class which logs time for a cache hit.
#define SCOPED_DAWN_CACHE_HIT_TIMER(platform, name) \
    SCOPED_DAWN_HISTOGRAM_TIMER(platform, "CacheHit." name)

// Scoped class which logs time for a cache miss.
#define SCOPED_DAWN_CACHE_MISS_TIMER(platform, name) \
    SCOPED_DAWN_HISTOGRAM_TIMER(platform, "CacheMiss." name)

// Wrapper for common use cases of Serializable::FromBlob that captures the platform and
// conditionally records a CacheHit if successful.
#define SCOPED_DAWN_CACHE_HIT_FROM_BLOB(platform, name, fromBlobFn)                     \
    [p = platform](Blob b) {                                                            \
        double start = p->MonotonicallyIncreasingTime();                                \
        auto result = fromBlobFn(std::move(b));                                         \
        if (result.IsSuccess()) {                                                       \
            int elapsedMS =                                                             \
                static_cast<int>((p->MonotonicallyIncreasingTime() - start) * 1'000.0); \
            DAWN_HISTOGRAM_TIMES(p, "CacheHit." name, elapsedMS);                       \
        }                                                                               \
        return result;                                                                  \
    }

#endif  // SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_MACROS_H_
