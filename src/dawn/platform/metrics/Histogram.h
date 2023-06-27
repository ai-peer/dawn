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

#ifndef SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_H_
#define SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_H_

#include <cstdint>

#include "dawn/platform/dawn_platform_export.h"

namespace dawn::platform {

class Platform;

namespace histogram {

DAWN_PLATFORM_EXPORT void CustomCounts(Platform* platform,
                                       const char* name,
                                       int sample,
                                       int min,
                                       int max,
                                       int bucketCount);

DAWN_PLATFORM_EXPORT void Enumeration(Platform* platform,
                                      const char* name,
                                      int sample,
                                      int boundaryValue);

DAWN_PLATFORM_EXPORT void Sparse(Platform* platform, const char* name, int sample);

DAWN_PLATFORM_EXPORT void Boolean(Platform* platform, const char* name, bool sample);

}  // namespace histogram
}  // namespace dawn::platform

#endif  // SRC_DAWN_PLATFORM_METRICS_HISTOGRAM_H_
