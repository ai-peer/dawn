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

#ifndef DAWNNATIVE_GPUINFO_H
#define DAWNNATIVE_GPUINFO_H

#include "dawn_native/DawnNative.h"

namespace dawn_native {

    bool IsAMD(PCIInfo pciInfo);
    bool IsARM(PCIInfo pciInfo);
    bool IsImgTec(PCIInfo pciInfo);
    bool IsIntel(PCIInfo pciInfo);
    bool IsNvidia(PCIInfo pciInfo);
    bool IsQualcomm(PCIInfo pciInfo);

}  // namespace dawn_native

#endif  // DAWNNATIVE_GPUINFO_H