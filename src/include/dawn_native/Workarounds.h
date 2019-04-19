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

#ifndef DAWNNATIVE_WORKAROUNDS_H_
#define DAWNNATIVE_WORKAROUNDS_H_

#include <array>
#include <bitset>

namespace dawn_native {

    enum class Workarounds {
        EmulateStoreAndMSAAResolve = 0,

        EnumCount = 1,
        InvalidEnum = EnumCount,
    };

    struct WorkaroundInfo {
        const char* workaroundString;
        const char* description;
        const char* url;

        static Workarounds WorkaroundStringToEnum(const char* workaroundString) {
            for (uint32_t i = 0; i < kDawnWorkarounds.size(); ++i) {
                const WorkaroundInfo& info = kDawnWorkarounds[i];
                if (strcmp(info.workaroundString, workaroundString) == 0) {
                    return static_cast<Workarounds>(i);
                }
            }

            return Workarounds::InvalidEnum;
        }
    };

    using WorkaroundsInfo = std::array<WorkaroundInfo, static_cast<size_t>(Workarounds::EnumCount)>;

    constexpr WorkaroundsInfo kDawnWorkarounds = {
        {{"emulate_store_and_msaa_resolve",

          "Emulate storing into multisampled color attachments and doing MSAA resolve "
          "simultaneously. This workaround is enabled by default on the Metal drivers that do not"
          "support MTLStoreActionStoreAndMultisampleResolve. To support StoreOp::Store on those "
          "platforms, we should do MSAA resolve in another render pass after ending the previous "
          "one.",

          "https://bugs.chromium.org/p/dawn/issues/detail?id=56"}}};

    struct WorkaroundsController {
        std::bitset<static_cast<size_t>(Workarounds::EnumCount)> forceEnabledWorkarounds;
        std::bitset<static_cast<size_t>(Workarounds::EnumCount)> forceDisabledWorkarounds;

        // Return a workaround should be used or not, and update the information if necessary.
        // - if the workaround has been set to force enabled or force disabled, return the
        //   information in forceEnabledWorkarounds and forceDisabledWorkarounds.
        // - else the information in forceEnabledWorkarounds and forceDisabledWorkarounds will
        //   be updated based on parameter isEnabled, and return isEnabled.
        bool ShouldWorkaroundBeUsed(Workarounds workaround, bool isEnabled) {
            const size_t kWorkaroundIndex = static_cast<size_t>(workaround);

            if (forceEnabledWorkarounds.test(kWorkaroundIndex)) {
                return true;
            }

            if (forceDisabledWorkarounds.test(kWorkaroundIndex)) {
                return false;
            }

            if (isEnabled) {
                forceEnabledWorkarounds.set(kWorkaroundIndex);
            } else {
                forceDisabledWorkarounds.set(kWorkaroundIndex);
            }

            return isEnabled;
        }
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_WORKAROUNDS_H_
