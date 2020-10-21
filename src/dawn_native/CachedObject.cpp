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

#include "dawn_native/CachedObject.h"
#include "dawn_native/FingerprintRecorder.h"

namespace dawn_native {

    bool CachedObject::IsCachedReference() const {
        return mIsCachedReference;
    }

    void CachedObject::SetIsCachedReference() {
        mIsCachedReference = true;
    }

    size_t RecordedObject::getKey() const {
        ASSERT(mKey != kEmptyKeyValue);
        return mKey;
    }

    size_t RecordedObject::HashFunc::operator()(const RecordedObject* obj) const {
        return obj->getKey();
    }

    bool RecordedObject::EqualityFunc::operator()(const RecordedObject* a,
                                                  const RecordedObject* b) const {
        return a->getKey() == b->getKey();
    }

}  // namespace dawn_native
