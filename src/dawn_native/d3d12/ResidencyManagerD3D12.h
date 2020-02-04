// Copyright 2020 The Dawn Authors
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

#ifndef DAWNNATIVE_D3D12_RESIDENCYMANAGERD3D12_H_
#define DAWNNATIVE_D3D12_RESIDENCYMANAGERD3D12_H_

#include "common/LinkedList.h"
#include "common/Serial.h"
#include "dawn_native/Error.h"
#include "dawn_native/d3d12/Forward.h"

#include <vector>

namespace dawn_native { namespace d3d12 {

    class Heap;

    class LRUEntry : public LinkNode<LRUEntry> {
      public:
        LRUEntry(uint64_t size);

        uint64_t GetSize() const;
        uint64_t GetLastSubmittedSerial() const;
        void SetLastSubmittedSerial(Serial serial);

        bool IsResident() const;

      private:
        Serial mLastUsedSerial = 0;
        uint64_t mSize = 0;
    };

    class ResidencyManager {
      public:
        ResidencyManager(Device* device);

        MaybeError EnsureCanMakeResident(uint64_t allocationSize);
        MaybeError EnsureHeapIsResident(Heap* heap);
        LRUEntry* Evict();
        MaybeError ProcessResidency(const std::vector<Heap*>& heapsPendingUsage);
        void TrackResidentAllocation(Heap* heap);

      private:
        Device* mDevice;
        bool mResidencyManagementEnabled;
        LinkedList<LRUEntry> mLRUCache;
    };

}}  // namespace dawn_native::d3d12

#endif  // DAWNNATIVE_D3D12_RESIDENCYMANAGERD3D12_H_