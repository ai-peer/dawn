// Copyright 2018 The Dawn Authors
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

#include "dawn_native/FenceSignalTracker.h"

#include "dawn_native/Fence.h"

namespace dawn_native {

    FenceSignalTracker::FenceSignalTracker() {
    }

    FenceSignalTracker::~FenceSignalTracker() {
        ASSERT(mFencesInFlight.Empty());
    }

    void FenceSignalTracker::UpdateFenceOnComplete(FenceBase* fence,
                                                   uint64_t value,
                                                   Serial serial) {
        mFencesInFlight.Enqueue(FenceInFlight{fence, value}, serial);
    }

    void FenceSignalTracker::Tick(Serial finishedSerial) {
        for (auto& fenceInFlight : mFencesInFlight.IterateUpTo(finishedSerial)) {
            if (fenceInFlight.fence->GetExternalRefs() == 0 &&
                fenceInFlight.fence->GetInternalRefs() == 1) {
                // The last reference to the fence is the internal reference in
                // this tracker. The fence should be destroyed. Do not update
                // the completed value which may trigger callbacks with SUCCESS.
                continue;
            }
            fenceInFlight.fence->SetCompletedValue(fenceInFlight.value);
        }
        mFencesInFlight.ClearUpTo(finishedSerial);
    }

}  // namespace dawn_native
