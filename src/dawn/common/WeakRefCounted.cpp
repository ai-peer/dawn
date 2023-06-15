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

#include "dawn/common/WeakRefCounted.h"

namespace dawn {
namespace detail {

WeakRefData::WeakRefData(RefCounted* value) : mValue(value) {}

void WeakRefData::Invalidate() {
  std::lock_guard<std::mutex> lock(mMutex);
  mIsValid = false;
  mValue = nullptr;
}

bool WeakRefData::IsValid() const {
  return mIsValid;
}

}  // namespace detail

WeakRefCounted::WeakRefCounted()  : mWeakRef(AcquireRef(new detail::WeakRefData(this))) {}

void WeakRefCounted::DeleteThis() {
  mWeakRef->Invalidate();
        RefCounted::DeleteThis();
}

}  // namespace dawn
