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

#ifndef SRC_DAWN_COMMON_WEAKREFCOUNTED_H_
#define SRC_DAWN_COMMON_WEAKREFCOUNTED_H_

#include "dawn/common/Ref.h"

namespace dawn {

template <typename T>
class WeakRef;

namespace detail {

// Interface base class used to enable compile-time verification of WeakRefCounted functions.
class WeakRefCountedBase {
  protected:
    explicit WeakRefCountedBase(detail::RefCountedData* data);
    ~WeakRefCountedBase();

  private:
    template <typename T>
    friend class ::dawn::WeakRef;

    Ref<detail::RefCountedData> mData;
};

}  // namespace detail

// Class that should be extended to enable WeakRefs for the type.
template <typename T>
class WeakRefCounted : public detail::WeakRefCountedBase {
  public:
    WeakRefCounted() : WeakRefCountedBase(new detail::RefCountedData(static_cast<T*>(this))) {}
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_WEAKREFCOUNTED_H_
