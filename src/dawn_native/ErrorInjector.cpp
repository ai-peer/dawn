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

#include "dawn_native/ErrorInjector.h"

#include "common/Assert.h"

namespace dawn_native {

    bool ErrorInjector::sIsEnabled = false;

    static uint64_t sNextIndex = 0;
    static uint64_t sInjectedFailureIndex = 0;
    static bool sHasPendingInjectedError = false;

    // static
    void ErrorInjector::Enable() {
        ErrorInjector::sIsEnabled = true;
    }

    // static
    void ErrorInjector::Disable() {
        ErrorInjector::sIsEnabled = false;
    }

    // static
    bool ErrorInjector::ShouldInjectErrorImpl() {
        uint64_t index = sNextIndex++;

        if (sHasPendingInjectedError && index == sInjectedFailureIndex) {
            sHasPendingInjectedError = false;
            return true;
        }

        return false;
    }

    // static
    void ErrorInjector::InjectErrorAt(uint64_t index) {
        // Only one error can be injected at a time.
        ASSERT(!sHasPendingInjectedError);

        sNextIndex = index;
        sHasPendingInjectedError = true;
    }

    // static
    uint64_t ErrorInjector::AcquireCallCounts() {
        uint64_t count = sNextIndex;
        Clear();
        return count;
    }

    // static
    void ErrorInjector::Clear() {
        sNextIndex = 0;
        sHasPendingInjectedError = false;
    }

}  // namespace dawn_native
