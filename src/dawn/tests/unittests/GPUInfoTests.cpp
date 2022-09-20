// Copyright 2021 The Dawn Authors
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

#include <gtest/gtest.h>

#include "dawn/common/GPUInfo.h"

namespace {

const PCIVendorID vendorID = 0x8086;
// Intel D3D
const gpu_info::DriverVersion version1({20, 19, 15, 5107});
const gpu_info::DriverVersion version2({21, 20, 16, 5077});
const gpu_info::DriverVersion version3({27, 20, 100, 9946});
const gpu_info::DriverVersion version4({27, 20, 101, 2003});
// Intel Vulkan
const gpu_info::DriverVersion version5({100, 9466});
const gpu_info::DriverVersion version6({101, 3222});
const gpu_info::DriverVersion version7({101, 3790});

}  // anonymous namespace

TEST(GPUInfo, CompareWindowsDriverVersion) {
    EXPECT_EQ(gpu_info::CompareWindowsDriverVersion(vendorID, version1, version2), -1);
    EXPECT_EQ(gpu_info::CompareWindowsDriverVersion(vendorID, version2, version3), -1);
    EXPECT_EQ(gpu_info::CompareWindowsDriverVersion(vendorID, version3, version4), -1);
    EXPECT_EQ(gpu_info::CompareWindowsDriverVersion(vendorID, version5, version6), -1);
    EXPECT_EQ(gpu_info::CompareWindowsDriverVersion(vendorID, version6, version7), -1);
}
