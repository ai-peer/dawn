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

#include <map>
#include <string>

#include "common/Assert.h"
#include "dawn_native/Toggles.h"

namespace dawn_native {

    Toggle ToggleNameToEnum(const char* toggleName) {
        const std::map<std::string, Toggle> kToggleStringToEnumMap = {
            {
                "emulate_store_and_msaa_resolve",
                Toggle::EmulateStoreAndMSAAResolve
            }
        };

        const auto iter = kToggleStringToEnumMap.find(toggleName);
        if (iter != kToggleStringToEnumMap.end()) {
            return iter->second;
        }

        return Toggle::InvalidEnum;
    }

    const char* ToggleEnumToName(Toggle toggle) {
        switch (toggle) {
            case Toggle::EmulateStoreAndMSAAResolve:
                return "emulate_store_and_msaa_resolve";
            default:
                UNREACHABLE();
        }
    }

    void TogglesSet::SetToggle(Toggle toggle, bool isEnabled) {
        ASSERT(toggle != Toggle::InvalidEnum);
        const size_t toggleIndex = static_cast<size_t>(toggle);
        toggleBitset.set(toggleIndex, isEnabled);
        availableToggleBitset.set(toggleIndex);
    }

    bool TogglesSet::IsValid(Toggle toggle) const {
        ASSERT(toggle != Toggle::InvalidEnum);
        const size_t toggleIndex = static_cast<size_t>(toggle);
        return availableToggleBitset.test(toggleIndex);
    }

    bool TogglesSet::IsEnabled(Toggle toggle) const {
        ASSERT(toggle != Toggle::InvalidEnum);
        const size_t toggleIndex = static_cast<size_t>(toggle);
        return toggleBitset.test(toggleIndex);
    }

    void TogglesSet::GetToggleInfo(ToggleInfo* info) const {
        ASSERT(info);

        Toggle toggleEnum = ToggleNameToEnum(info->name);
        if (toggleEnum == Toggle::InvalidEnum) {
            info->description = "";
            info->url = "";
            info->isEnabled = false;
            info->isValid = false;
            return;
        }

        const size_t toggleIndex = static_cast<size_t>(toggleEnum);

        info->isEnabled = toggleBitset.test(toggleIndex);
        info->isValid = availableToggleBitset.test(toggleIndex);

        switch (toggleEnum) {
            case Toggle::EmulateStoreAndMSAAResolve:
                info->description =
                    "Emulate storing into multisampled color attachments and doing MSAA resolve "
                    "simultaneously. This workaround is enabled by default on the Metal drivers "
                    "that do not support MTLStoreActionStoreAndMultisampleResolve. To support "
                    "StoreOp::Store on those platforms, we should do MSAA resolve in another render"
                    " pass after ending the previous one.";
                info->url = "https://bugs.chromium.org/p/dawn/issues/detail?id=56";
                break;

            default:
                UNREACHABLE();
        }
    }

}  // namespace dawn_native
