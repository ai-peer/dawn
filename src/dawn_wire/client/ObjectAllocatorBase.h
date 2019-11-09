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

#ifndef DAWNWIRE_CLIENT_OBJECTALLOCATORBASE_H_
#define DAWNWIRE_CLIENT_OBJECTALLOCATORBASE_H_

#include <vector>

namespace dawn_wire {
    enum class ObjectType : uint32_t;
}

namespace dawn_wire { namespace client {

    class Client;
    class ClientBase;

    class ObjectAllocatorBase {
      public:
        ObjectAllocatorBase(ClientBase* client);

      protected:
        uint32_t GetNewId();
        void FreeId(uint32_t id);
        void EnqueueDestroy(ObjectType objectType, uint32_t id);

      private:
        // 0 is an ID reserved to represent nullptr
        uint32_t mCurrentId = 1;
        std::vector<uint32_t> mFreeIds;
        Client* mClient;
    };

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_OBJECTALLOCATORBASE_H_
