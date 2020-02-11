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

#ifndef DAWNNATIVE_OBJECTHANDLEPOOL_H_
#define DAWNNATIVE_OBJECTHANDLEPOOL_H_

#include <cstdlib>

namespace dawn_native {

    class ObjectHandleBase;

    class ObjectHandlePool {
      public:
        ObjectHandlePool();
        ~ObjectHandlePool();

        size_t Size() const;
        void Shrink(size_t size);

        ObjectHandleBase* Pop();
        void Push(ObjectHandleBase* handle);

      private:
        ObjectHandleBase* mHead;
        size_t mSize;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_OBJECTHANDLEPOOL_H_
