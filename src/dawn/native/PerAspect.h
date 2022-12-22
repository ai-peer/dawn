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

#ifndef SRC_DAWN_NATIVE_PERASPECT_H_
#define SRC_DAWN_NATIVE_PERASPECT_H_

#include <utility>

#include "dawn/common/Assert.h"
#include "dawn/common/ityp_array.h"
#include "dawn/native/Subresource.h"

namespace dawn::native {

template <typename T>
class PerAspect {
  public:
    PerAspect() = default;

    explicit PerAspect(Aspect aspectMask) : mAspectMask(aspectMask) {}

    PerAspect(Aspect aspect, T value) : mAspectMask(aspect) {
        ASSERT(HasOneBit(aspect));
        this->operator[](aspect) = std::move(value);
    }

    PerAspect(std::initializer_list<std::pair<Aspect, T>> entries) : mAspectMask(Aspect::None) {
        for (auto& e : entries) {
            ASSERT(HasOneBit(e.first));
            mAspectMask |= e.first;
            (*this)[e.first] = std::move(e.second);
        }
    }

    T& operator[](Aspect aspect) {
        ASSERT(mAspectMask & aspect);
        return mData[GetAspectIndex(aspect)];
    }
    const T& operator[](Aspect aspect) const {
        ASSERT(mAspectMask & aspect);
        return mData[GetAspectIndex(aspect)];
    }

    Aspect GetAspects() const { return mAspectMask; }

  private:
    Aspect mAspectMask;
    std::array<T, 2> mData;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_PERASPECT_H_
