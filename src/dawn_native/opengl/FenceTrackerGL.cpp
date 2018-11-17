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

#include "dawn_native/opengl/FenceTrackerGL.h"

#include "dawn_native/opengl/FenceGL.h"

namespace dawn_native { namespace opengl {

    FenceTracker::FenceTracker() {
    }

    FenceTracker::~FenceTracker() {
        ASSERT(mFencesInFlight.empty());
    }

    void FenceTracker::UpdateFenceOnComplete(GLsync sync, Fence* fence, uint64_t value) {
        mFencesInFlight.emplace_back(FenceInFlight{sync, fence, value});
    }

    void FenceTracker::Tick() {
        auto it = mFencesInFlight.begin();
        for (; it != mFencesInFlight.end(); ++it) {
            GLint status = 0;
            GLsizei length;
            glGetSynciv(it->sync, GL_SYNC_CONDITION, sizeof(GLint), &length, &status);
            ASSERT(length == 1);

            if (status) {
                glDeleteSync(it->sync);
                it->fence->SetCompletedValue(it->value);
            } else {
                // Fences are enqueued in order.
                // No other fences will be complete.
                break;
            }
        }
        mFencesInFlight.erase(mFencesInFlight.begin(), it);
    }

}}  // namespace dawn_native::opengl
