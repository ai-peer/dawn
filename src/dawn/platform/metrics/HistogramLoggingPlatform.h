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

// This header provides macros for adding Chromium UMA histogram stats.
// See the detailed description in the Chromium codebase:
// https://source.chromium.org/chromium/chromium/src/+/main:base/metrics/histogram_macros.h

#ifndef SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_LOGGING_PLATFORM_H_
#define SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_LOGGING_PLATFORM_H_

#include "dawn/platform/DawnPlatform.h"

namespace dawn::platform {

// Platform implementation that logs histogram functions to stdout.
// Useful for implementing and live-testing histogram function calls.
class DAWN_PLATFORM_EXPORT HistogramLoggingPlatform : public Platform {
  public:
    double MonotonicallyIncreasingTime() override;

    // Invoked to add a UMA histogram count-based sample
    void HistogramCustomCounts(const char* name,
                               int sample,
                               int min,
                               int max,
                               int bucketCount) override;

    // Invoked to add a UMA histogram count-based sample that requires high-performance
    // counter (HPC) support.
    void HistogramCustomCountsHPC(const char* name,
                               int sample,
                               int min,
                               int max,
                               int bucketCount) override;

    // Invoked to add a UMA histogram enumeration sample
    void HistogramEnumeration(const char* name, int sample, int boundaryValue) override;

    // Invoked to add a UMA histogram sparse sample
    void HistogramSparse(const char* name, int sample) override;

    // Invoked to add a UMA histogram boolean sample
    void HistogramBoolean(const char* name, bool sample) override;
};

}  // namespace dawn::platform

#endif  // SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_LOGGING_PLATFORM_H_
