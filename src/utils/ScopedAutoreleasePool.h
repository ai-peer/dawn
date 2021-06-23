// Copyright 2021 The Dawn Authors
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

#ifndef UTILS_SCOPEDAUTORELEASEPOOL_H_
#define UTILS_SCOPEDAUTORELEASEPOOL_H_

#include "common/Compiler.h"

#include <cstddef>

namespace utils {

    class DAWN_NO_DISCARD ScopedAutoreleasePool {
      public:
        ScopedAutoreleasePool(std::nullptr_t);
        ScopedAutoreleasePool();
        ~ScopedAutoreleasePool();

        ScopedAutoreleasePool(const ScopedAutoreleasePool&) = delete;
        ScopedAutoreleasePool& operator=(const ScopedAutoreleasePool&) = delete;

        ScopedAutoreleasePool(ScopedAutoreleasePool&&);
        ScopedAutoreleasePool& operator=(ScopedAutoreleasePool&&);

      private:
        void* mPool = nullptr;
    };

}  // namespace utils

#endif  // UTILS_SCOPEDAUTORELEASEPOOL_H_
