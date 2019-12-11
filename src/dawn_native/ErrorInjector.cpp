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
#include "common/HashUtils.h"

namespace dawn_native {

    ErrorInjector* ErrorInjector::gInjector = nullptr;

    ErrorInjector::ErrorInjector() = default;

    ErrorInjector::~ErrorInjector() = default;

    // static
    void ErrorInjector::Set(ErrorInjector* injector) {
        gInjector = injector;
    }

    bool ErrorInjector::ShouldInjectErrorImpl(const char* file, const char* func, int line) {
        size_t callsite = 0;
        HashCombine(&callsite, file, func, line);
        uint64_t index = mCallCounts[callsite]++;

        if (mHasPendingInjectedError && mInjectedCallsiteFailure == callsite &&
            index == mInjectedCallsiteFailureIndex) {
            mHasPendingInjectedError = false;
            return true;
        }

        return false;
    }

    void ErrorInjector::InjectErrorAt(size_t callsite, uint64_t index) {
        // Only one error can be injected at a time.
        ASSERT(!mHasPendingInjectedError);

        mInjectedCallsiteFailure = callsite;
        mInjectedCallsiteFailureIndex = index;
        mHasPendingInjectedError = true;
    }

    std::unordered_map<size_t, uint64_t> ErrorInjector::AcquireCallLog() {
        return std::move(mCallCounts);
    }

    void ErrorInjector::Clear() {
        mCallCounts.clear();
        mHasPendingInjectedError = false;
    }

}  // namespace dawn_native
