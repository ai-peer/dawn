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

#include "dawn_native/ObjectHandlePool.h"

#include "common/Assert.h"
#include "dawn_native/ObjectHandle.h"

namespace dawn_native {

    ObjectHandlePool::ObjectHandlePool() : mHead(nullptr), mSize(0) {

    }

    ObjectHandlePool::~ObjectHandlePool() {
        Shrink(0);
    }

    size_t ObjectHandlePool::Size() const {
        return mSize;
    }

    void ObjectHandlePool::Shrink(size_t size) {
        ASSERT(mSize >= size);
        while (mSize > size) {
            Pop()->Free();
        }
    }

    ObjectHandleBase* ObjectHandlePool::Pop() {
        if (mSize == 0) {
            return nullptr;
        }

        // Pop the handle off of the linked list.
        ASSERT(mHead != nullptr);
        ObjectHandleBase* handle = mHead;
        mHead = mHead->mNextHandle;
        handle->mNextHandle = nullptr;

        mSize--;
        return handle;
    }

    void ObjectHandlePool::Push(ObjectHandleBase* handle) {
        // Only single handles should be pushed, not a chain.
        ASSERT(handle->mNextHandle == nullptr);

        // Recycled handles should have their storage deleted.
        ASSERT(handle->mStorage == nullptr);

        handle->mNextHandle = mHead;
        mHead = handle;

        mSize++;
    }

}  // namespace dawn_native
