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

// histogram_macros.h:
//   Helpers for making histograms, to keep consistency with Chromium's
//   histogram_macros.h.

#ifndef SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_MACROS_H_
#define SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_MACROS_H_

#include "dawn/platform/metrics/Histogram.h"

#define DAWN_HISTOGRAM_TIMES(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_TIMES(platform, name, sample, 1, 10000, 50)

#define DAWN_HISTOGRAM_MEDIUM_TIMES(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_TIMES(platform, name, sample, 10, 180000, 50)

// Use this macro when times can routinely be much longer than 10 seconds.
#define DAWN_HISTOGRAM_LONG_TIMES(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_TIMES(platform, name, sample, 1, 3600000, 50)

// Use this macro when times can routinely be much longer than 10 seconds and
// you want 100 buckets.
#define DAWN_HISTOGRAM_LONG_TIMES_100(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_TIMES(platform, name, sample, 1, 3600000, 100)

// For folks that need real specific times, use this to select a precise range
// of times you want plotted, and the number of buckets you want used.
#define DAWN_HISTOGRAM_CUSTOM_TIMES(platform, name, sample, min, max, bucket_count) \
    DAWN_HISTOGRAM_CUSTOM_COUNTS(platform, name, sample, min, max, bucket_count)

#define DAWN_HISTOGRAM_COUNTS(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_COUNTS(platform, name, sample, 1, 1000000, 50)

#define DAWN_HISTOGRAM_COUNTS_100(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_COUNTS(platform, name, sample, 1, 100, 50)

#define DAWN_HISTOGRAM_COUNTS_10000(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_COUNTS(platform, name, sample, 1, 10000, 50)

#define DAWN_HISTOGRAM_CUSTOM_COUNTS(platformObj, name, sample, min, max, bucket_count) \
    dawn::platform::histogram::CustomCounts(platformObj, name, sample, min, max, bucket_count)

#define DAWN_HISTOGRAM_PERCENTAGE(platform, name, under_one_hundred) \
    DAWN_HISTOGRAM_ENUMERATION(platform, name, under_one_hundred, 101)

#define DAWN_HISTOGRAM_BOOLEAN(platformObj, name, sample) \
    dawn::platform::histogram::Boolean(platformObj, name, sample)

#define DAWN_HISTOGRAM_ENUMERATION(platformObj, name, sample, boundary_value) \
    dawn::platform::histogram::Enumeration(platformObj, name, sample, boundary_value)

#define DAWN_HISTOGRAM_MEMORY_KB(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_COUNTS(platform, name, sample, 1000, 500000, 50)

#define DAWN_HISTOGRAM_MEMORY_MB(platform, name, sample) \
    DAWN_HISTOGRAM_CUSTOM_COUNTS(platform, name, sample, 1, 1000, 50)

#define DAWN_HISTOGRAM_SPARSE_SLOWLY(platformObj, name, sample) \
    dawn::platform::histogram::Sparse(platformObj, name, sample)

// Scoped class which logs its time on this earth as a UMA statistic. This is
// recommended for when you want a histogram which measures the time it takes
// for a method to execute. This measures up to 10 seconds.
#define SCOPED_DAWN_HISTOGRAM_TIMER(platform, name) \
    SCOPED_DAWN_HISTOGRAM_TIMER_EXPANDER(platform, name, false, __COUNTER__)

// Similar scoped histogram timer, but this uses DAWN_HISTOGRAM_LONG_TIMES_100,
// which measures up to an hour, and uses 100 buckets. This is more expensive
// to store, so only use if this often takes >10 seconds.
#define SCOPED_DAWN_HISTOGRAM_LONG_TIMER(platform, name) \
    SCOPED_DAWN_HISTOGRAM_TIMER_EXPANDER(platform, name, true, __COUNTER__)

// This nested macro is necessary to expand __COUNTER__ to an actual value.
#define SCOPED_DAWN_HISTOGRAM_TIMER_EXPANDER(platform, name, is_long, key) \
    SCOPED_DAWN_HISTOGRAM_TIMER_UNIQUE(platform, name, is_long, key)

#define SCOPED_DAWN_HISTOGRAM_TIMER_UNIQUE(platform, name, is_long, key)                    \
    using PlatformType##key = std::decay_t<std::remove_pointer_t<decltype(platform)>>;      \
    class [[nodiscard]] ScopedHistogramTimer##key {                                         \
      public:                                                                               \
        using Platform = PlatformType##key;                                                 \
        ScopedHistogramTimer##key(Platform* p)                                              \
            : platform_(p), constructed_(platform_->MonotonicallyIncreasingTime()) {}       \
        ~ScopedHistogramTimer##key() {                                                      \
            if (constructed_ == 0)                                                          \
                return;                                                                     \
            double elapsed = this->platform_->MonotonicallyIncreasingTime() - constructed_; \
            int elapsedMS = static_cast<int>(elapsed * 1000.0);                             \
            if (is_long) {                                                                  \
                DAWN_HISTOGRAM_LONG_TIMES_100(platform_, name, elapsedMS);                  \
            } else {                                                                        \
                DAWN_HISTOGRAM_TIMES(platform_, name, elapsedMS);                           \
            }                                                                               \
        }                                                                                   \
                                                                                            \
      private:                                                                              \
        Platform* platform_;                                                                \
        double constructed_;                                                                \
    } scoped_histogram_timer_##key(platform)

#endif  // SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_MACROS_H_
