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

#include "dawn_native/GPUInfo.h"

#include "common/Constants.h"

namespace dawn_native {

    bool IsAMD(PCIInfo pciInfo) {
        return pciInfo.vendorId == kVendorID_AMD;
    }
    bool IsARM(PCIInfo pciInfo) {
        return pciInfo.vendorId == kVendorID_ARM;
    }
    bool IsImgTec(PCIInfo pciInfo) {
        return pciInfo.vendorId == kVendorID_ImgTec;
    }
    bool IsIntel(PCIInfo pciInfo) {
        return pciInfo.vendorId == kVendorID_Intel;
    }
    bool IsNvidia(PCIInfo pciInfo) {
        return pciInfo.vendorId == kVendorID_Nvidia;
    }
    bool IsQualcomm(PCIInfo pciInfo) {
        return pciInfo.vendorId == kVendorID_Qualcomm;
    }

}  // namespace dawn_native
