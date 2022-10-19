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

#include "dawn/wire/ChunkedCommandSerializer.h"

namespace dawn::wire {

namespace {

class NoopCommandSerializer final : public CommandSerializer {
  public:
    static NoopCommandSerializer* GetInstance() {
        static NoopCommandSerializer gNoopCommandSerializer;
        return &gNoopCommandSerializer;
    }

    ~NoopCommandSerializer() override = default;

    size_t GetMaximumAllocationSize() const final { return 0; }
    void* GetCmdSpace(size_t size) final { return nullptr; }
    bool Flush() final { return false; }
};

}  // anonymous namespace

ChunkedCommandSerializer::ChunkedCommandSerializer(CommandSerializer* serializer)
    : mSerializer(serializer), mMaxAllocationSize(serializer->GetMaximumAllocationSize()) {}

void ChunkedCommandSerializer::SerializeChunkedCommand(const char* allocatedBuffer,
                                                       size_t remainingSize) {
    const std::lock_guard<std::mutex> lock(mMutex);
    while (remainingSize > 0) {
        size_t chunkSize = std::min(remainingSize, mMaxAllocationSize);
        void* dst = mSerializer->GetCmdSpace(chunkSize);
        if (dst == nullptr) {
            mSerializer->DidWriteCmds(0);
            return;
        }
        memcpy(dst, allocatedBuffer, chunkSize);
        mSerializer->DidWriteCmds(chunkSize);

        allocatedBuffer += chunkSize;
        remainingSize -= chunkSize;
    }
}

void ChunkedCommandSerializer::Disconnect() {
    const std::lock_guard<std::mutex> lock(mMutex);
    mSerializer = NoopCommandSerializer::GetInstance();
}

}  // namespace dawn::wire
