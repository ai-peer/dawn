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

#ifndef SRC_DAWN_COMMON_GPUINFO_H_
#define SRC_DAWN_COMMON_GPUINFO_H_

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "dawn/common/GPUInfo_autogen.h"
#include "dawn/common/ityp_stack_vec.h"

namespace gpu_info {

template <typename Index, typename Value, size_t StaticCapacity>
class DriverVersionVector : public ityp::stack_vec<Index, Value, StaticCapacity> {
    using Base = ityp::stack_vec<Index, Value, StaticCapacity>;

  public:
    DriverVersionVector() : Base() {}
    explicit DriverVersionVector(const std::vector<Value>& ver) : Base(ver) {}

    std::string toString() const {
        std::ostringstream oss;
        if (this->size() > 0) {
            // Convert all but the last element to avoid a trailing "."
            std::copy(this->begin(), this->end() - 1, std::ostream_iterator<Value>(oss, "."));
            // Add the last element
            oss << this->back();
        }

        return oss.str();
    }
};

// Four uint16 fields could cover almost all driver version schemas:
// D3D12: AA.BB.CCC.DDDD
// Vulkan: AAA.BBB.CCC.DDD on Nvidia, CCC.DDDD for Intel Windows, and AA.BB.CCC for others,
// See https://vulkan.gpuinfo.org/
static constexpr uint32_t kMaxVersionFields = 4;
using DriverVersion = DriverVersionVector<uint32_t, uint16_t, kMaxVersionFields>;

// Do comparison between two driver versions. Currently we only support the comparison between
// Intel Windows driver versions.
// - Return -1 if build number of version1 is smaller
// - Return 1 if build number of version1 is bigger
// - Return 0 if version1 and version2 represent same driver version
int CompareWindowsDriverVersion(PCIVendorID vendorId,
                                const DriverVersion& version1,
                                const DriverVersion& version2);

// Intel architectures
bool IsSkylake(PCIDeviceID deviceId);

}  // namespace gpu_info
#endif  // SRC_DAWN_COMMON_GPUINFO_H_
