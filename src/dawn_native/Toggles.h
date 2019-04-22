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

#ifndef DAWNNATIVE_TOGGLES_H_
#define DAWNNATIVE_TOGGLES_H_

#include <bitset>

#include "dawn_native/DawnNative.h"

namespace dawn_native {

    enum class Toggle {
        EmulateStoreAndMSAAResolve = 0,

        EnumCount = 1,
        InvalidEnum = EnumCount,
    };

    Toggle ToggleNameToEnum(const char* toggleName);

    const char* ToggleEnumToName(Toggle toggle);

    struct TogglesSet {
        std::bitset<static_cast<size_t>(Toggle::EnumCount)> toggleBitset;
        std::bitset<static_cast<size_t>(Toggle::EnumCount)> availableToggleBitset;

        void SetToggle(Toggle toggle, bool enabled);
        bool IsValid(Toggle toggle) const;
        bool IsEnabled(Toggle toggle) const;

        // Query the details of the toggle whose name is info->name.
        void GetToggleInfo(ToggleInfo* info) const;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_TOGGLESk_H_
