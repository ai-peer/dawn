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

#ifndef SRC_DAWN_WIRE_WIREDESERIALIZEALLOCATOR_H_
#define SRC_DAWN_WIRE_WIREDESERIALIZEALLOCATOR_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dawn/wire/WireCmd_autogen.h"

namespace dawn::wire {
// A really really simple implementation of the DeserializeAllocator. It's main feature
// is that it has some inline storage so as to avoid allocations for the majority of
// commands.
class WireDeserializeAllocator : public DeserializeAllocator {
  public:
    WireDeserializeAllocator();
    virtual ~WireDeserializeAllocator();

    void* GetSpace(size_t size, FutureID futureID) override;

    // Marks all space associated with the future as no longer used and ready to be reclaimed.
    void FreeFuture(FutureID futureID);

    // Resets all allocation and memory unrelated to futures.
    void Reset();

  private:
    static constexpr size_t kAllocationSize = 2048;

    size_t mRemainingSize = 0;
    char* mCurrentBuffer = nullptr;
    char mStaticBuffer[kAllocationSize];
    std::vector<char*> mAllocations;

    // Future allocations are tracked separately for now since we have both types. The important
    // thing about future allocations are that they should not be cleaned up until after the
    // callback. The cleanup will be enforced by the EventManager.
    size_t mRemainingFutureSize = 0;
    char* mCurrentFutureBuffer = nullptr;
    std::unordered_map<FutureID, std::unordered_set<char*>> mFutureToAllocations;
    std::unordered_map<char*, std::unordered_set<FutureID>> mAllocationToFutures;
};
}  // namespace dawn::wire

#endif  // SRC_DAWN_WIRE_WIREDESERIALIZEALLOCATOR_H_
