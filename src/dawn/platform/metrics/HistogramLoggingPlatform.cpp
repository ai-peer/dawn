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

#include "dawn/platform/metrics/HistogramLoggingPlatform.h"

#include <iostream>

#include "dawn/utils/Timer.h"

namespace dawn::platform {

double HistogramLoggingPlatform::MonotonicallyIncreasingTime() {
    static utils::Timer* timer = utils::CreateTimer();
    return timer->GetAbsoluteTime();
}

void HistogramLoggingPlatform::HistogramCustomCounts(const char* name,
                                                     int sample,
                                                     int min,
                                                     int max,
                                                     int bucketCount) {
    std::cout << "[HistogramCustomCounts] name: " << name << ", sample: " << sample
              << ", min: " << min << ", max: " << max << ", bucketCount: " << bucketCount
              << std::endl;
}

void HistogramLoggingPlatform::HistogramCustomCountsHPC(const char* name,
                                                        int sample,
                                                        int min,
                                                        int max,
                                                        int bucketCount) {
    std::cout << "[HistogramCustomCountsHPC] name: " << name << ", sample: " << sample
              << ", min: " << min << ", max: " << max << ", bucketCount: " << bucketCount
              << std::endl;
}

void HistogramLoggingPlatform::HistogramEnumeration(const char* name,
                                                    int sample,
                                                    int boundaryValue) {
    std::cout << "[HistogramEnumeration] name: " << name << ", sample: " << sample
              << ", boundaryValue: " << boundaryValue << std::endl;
}

void HistogramLoggingPlatform::HistogramSparse(const char* name, int sample) {
    std::cout << "[HistogramSparse] name: " << name << ", sample: " << sample << std::endl;
}

void HistogramLoggingPlatform::HistogramBoolean(const char* name, bool sample) {
    std::cout << "[HistogramBoolean] name: " << name << ", sample: " << sample << std::endl;
}

}  // namespace dawn::platform
