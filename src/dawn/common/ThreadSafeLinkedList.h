
// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_COMMON_THREADSAFELINKEDLIST_H_
#define SRC_DAWN_COMMON_THREADSAFELINKEDLIST_H_

#include "dawn/common/LinkedList.h"
#include "dawn/common/Lock.h"
#include "dawn/common/Mutex.h"
#include "dawn/common/ThreadSafety.h"

namespace dawn {

template <typename T>
class ThreadSafeLinkedList;

// ThreadSafeLinkNode is a stripped down version of LinkNode that removes functionality that
// is not thread safe. Operations must be done through the owning ThreadSafeLinkedList which
// first acquire the list's mutex.
template <typename T>
class ThreadSafeLinkNode : private LinkNode<T> {
  public:
    ~ThreadSafeLinkNode() {
        // Note: this check will race if there is an existing use-after-free bug where the node is
        // being removed or added to a list at the same time that it is deleted.
        ASSERT(!LinkNode<T>::IsInList());
    }
    using LinkNode<T>::value;

  private:
    friend class LinkNode<T>;
    friend class ThreadSafeLinkedList<T>;
};

// ThreadSafeLinkedList is a synchronized wrapped around LinkedList
template <typename T>
class ThreadSafeLinkedList {
  public:
    // Appends |e| to the end of the linked list.
    void Append(ThreadSafeLinkNode<T>* e) {
        Lock lock(mMutex);
        mList.Append(e);
    }

    // Prepends |e| to the front of the linked list.
    void Prepend(ThreadSafeLinkNode<T>* e) {
        Lock lock(mMutex);
        mList.Prepend(e);
    }

    // Remove |e| from the linked list. Returns true iff removed from a list.
    // |e| must be in |this|, or not in any list.
    bool Remove(ThreadSafeLinkNode<T>* e) {
        Lock lock(mMutex);
        return mList.Remove(e);
    }

    // Move all the contents of the linked list into |list|.
    void MoveInto(LinkedList<T>* list) {
        Lock lock(mMutex);
        mList.MoveInto(list);
    }

  private:
    Mutex mMutex;
    LinkedList<T> mList GUARDED_BY(mMutex);
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_THREADSAFELINKEDLIST_H_
