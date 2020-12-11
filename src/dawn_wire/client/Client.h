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

#ifndef DAWNWIRE_CLIENT_CLIENT_H_
#define DAWNWIRE_CLIENT_CLIENT_H_

#include <dawn/webgpu.h>
#include <dawn_wire/Wire.h>

#include "common/LinkedList.h"
#include "dawn_wire/ChunkedCommandSerializer.h"
#include "dawn_wire/WireClient.h"
#include "dawn_wire/WireCmd_autogen.h"
#include "dawn_wire/WireDeserializeAllocator.h"
#include "dawn_wire/client/ClientBase_autogen.h"
#include "dawn_wire/client/Instance.h"

namespace dawn_wire { namespace client {

    class Adapter;
    class Device;
    class Instance;
    class MemoryTransferService;
    class Surface;

    class Client : public ClientBase {
      public:
        Client(CommandSerializer* serializer, MemoryTransferService* memoryTransferService);
        ~Client() override;

        // ChunkedCommandHandler implementation
        const volatile char* HandleCommandsImpl(const volatile char* commands,
                                                size_t size) override;

        WGPUInstance GetInstance();

        MemoryTransferService* GetMemoryTransferService() const {
            return mMemoryTransferService;
        }

        template <typename T>
        T* FindAncestor() {
            static_assert(std::is_same<T, Client>::value, "");
            return reinterpret_cast<T*>(this);
        }

        template <typename T>
        const T* FindAncestor() const {
            static_assert(std::is_same<T, Client>::value, "");
            return reinterpret_cast<const T*>(this);
        }

        ReservedTexture ReserveTexture(WGPUDevice device);

        template <typename Cmd>
        void SerializeCommand(const Cmd& cmd) {
            mSerializer.SerializeCommand(cmd, *this);
        }

        template <typename Cmd, typename ExtraSizeSerializeFn>
        void SerializeCommand(const Cmd& cmd,
                              size_t extraSize,
                              ExtraSizeSerializeFn&& SerializeExtraSize) {
            mSerializer.SerializeCommand(cmd, *this, extraSize, SerializeExtraSize);
        }

        void Disconnect();
        bool IsDisconnected() const;

        void TrackObject(Instance* instance);
        void TrackObject(Adapter* adapter);
        void TrackObject(Device* device);
        void TrackObject(Surface* surface);

      private:
        void DestroyAllObjects();

#include "dawn_wire/client/ClientPrototypes_autogen.inc"

        LinkedList<ObjectBase> mInstances;
        LinkedList<ObjectBase> mAdapters;
        LinkedList<ObjectBase> mDevices;

        Instance* mInstance;
        ChunkedCommandSerializer mSerializer;
        MemoryTransferService* mMemoryTransferService;
        WireDeserializeAllocator mAllocator;
        std::unique_ptr<MemoryTransferService> mOwnedMemoryTransferService = nullptr;

        bool mDisconnected = false;
    };

    std::unique_ptr<MemoryTransferService> CreateInlineMemoryTransferService();

}}  // namespace dawn_wire::client

#endif  // DAWNWIRE_CLIENT_CLIENT_H_
