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
    double timestamp = platform->MonotonicallyIncreasingTime();
    if (timestamp != 0) {
        platform->HistogramCustomCounts(name, sample, min, max, bucketCount);
    }
}

void Enumeration(Platform* platform, const char* name, int sample, int boundaryValue) {
    ASSERT(platform != nullptr);
    double timestamp = platform->MonotonicallyIncreasingTime();
    if (timestamp != 0) {
        platform->HistogramEnumeration(name, sample, boundaryValue);
    }
}

void Sparse(Platform* platform, const char* name, int sample) {
    ASSERT(platform != nullptr);
    double timestamp = platform->MonotonicallyIncreasingTime();
    if (timestamp != 0) {
        platform->HistogramSparse(name, sample);
    }
}

void Boolean(Platform* platform, const char* name, bool sample) {
    ASSERT(platform != nullptr);
    double timestamp = platform->MonotonicallyIncreasingTime();
    if (timestamp != 0) {
        platform->HistogramBoolean(name, sample);
    }
}

}  // namespace dawn::platform::histogram
