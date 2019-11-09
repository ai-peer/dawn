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

#include "dawn_wire/client/Client.h"
#include "dawn_wire/client/Device.h"

#include <numeric>

namespace dawn_wire { namespace client {

    static ObjectHandle dummy;

    Client::Client(CommandSerializer* serializer, MemoryTransferService* memoryTransferService)
        : ClientBase(),
          mDevice(DeviceAllocator().New(this, &dummy)->object.get()),
          mSerializer(serializer),
          mMemoryTransferService(memoryTransferService) {
        if (mMemoryTransferService == nullptr) {
            // If a MemoryTransferService is not provided, fall back to inline memory.
            mOwnedMemoryTransferService = CreateInlineMemoryTransferService();
            mMemoryTransferService = mOwnedMemoryTransferService.get();
        }
    }

    Client::~Client() {
        DeviceAllocator().Free(mDevice);
    }

    ReservedTexture Client::ReserveTexture(WGPUDevice cDevice) {
        Device* device = reinterpret_cast<Device*>(cDevice);

        ObjectHandle handle = {};
        ObjectAllocator<Texture>::ObjectAndSerial* allocation = TextureAllocator().New(device, &handle);

        ReservedTexture result;
        result.texture = reinterpret_cast<WGPUTexture>(allocation->object.get());
        result.id = handle.id;
        result.generation = handle.serial;
        return result;
    }

    void Client::EnqueueDestroy(ObjectType objectType, uint32_t id) {
        mObjectsToDestroy[static_cast<uint32_t>(objectType)].push_back(id);
        mTotalObjectsToDestroy++;
        if (mTotalObjectsToDestroy > 1000000) {
            Flush();
        }
    }

    bool Client::Flush() {
        std::array<uint32_t, kObjectTypeCount> types;
        std::iota(types.begin(), types.end(), 0);

        std::make_heap(types.begin(), types.end(), [&](uint32_t a, uint32_t b) {
            return mObjectsToDestroy[a].size() < mObjectsToDestroy[b].size();
        });

        uint32_t maxToDestroy = std::min(mTotalObjectsToDestroy, mTotalObjectsToDestroy / 10 + 1000);

        uint32_t totalDestroyCount = 0;

        for (uint32_t type : types) {
            uint32_t cursor = mObjectsToDestroy[type].size();
            uint32_t destroyCursor = cursor;

            while (cursor > 0 && totalDestroyCount < maxToDestroy) {
                uint32_t id = mObjectsToDestroy[type][--cursor];
                if (AcquireNeedsDestroy(static_cast<ObjectType>(type), id)) {
                    mObjectsToDestroy[type][--destroyCursor] = id;
                    totalDestroyCount++;
                }
            }

            uint32_t count = mObjectsToDestroy[type].size() - destroyCursor;
            if (count > 0) {
                DestroyObjectsCmd cmd;
                cmd.objectType = static_cast<ObjectType>(type);
                cmd.count = count;
                cmd.objectIds = &mObjectsToDestroy[type][destroyCursor];

                size_t requiredSize = cmd.GetRequiredSize();
                char* allocatedBuffer =
                    static_cast<char*>(GetCmdSpace(requiredSize));
                cmd.Serialize(allocatedBuffer);

                mObjectsToDestroy[type].erase(
                    mObjectsToDestroy[type].end() - (mObjectsToDestroy[type].size() - cursor),
                    mObjectsToDestroy[type].end());
            }
            mTotalObjectsToDestroy -= count;

            if (totalDestroyCount >= maxToDestroy)
                break;
        }
        return mSerializer->Flush();
    }

}}  // namespace dawn_wire::client
