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

#include "dawn/platform/metrics/Histogram.h"
#include "dawn/common/Assert.h"
#include "dawn/platform/DawnPlatform.h"

namespace dawn::platform::histogram {

void CustomCounts(Platform* platform,
                  const char* name,
                  int sample,
                  int min,
                  int max,
                  int bucketCount) {
    ASSERT(platform != nullptr);
    platform->HistogramCustomCounts(name, sample, min, max, bucketCount);
}

void Enumeration(Platform* platform, const char* name, int sample, int boundaryValue) {
    ASSERT(platform != nullptr);
    platform->HistogramEnumeration(name, sample, boundaryValue);
}

void Sparse(Platform* platform, const char* name, int sample) {
    ASSERT(platform != nullptr);
    platform->HistogramSparse(name, sample);
}

void Boolean(Platform* platform, const char* name, bool sample) {
    ASSERT(platform != nullptr);
    platform->HistogramBoolean(name, sample);
}

}  // namespace dawn::platform::histogram
