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

#include "dawn_native/FingerprintRecorder.h"

namespace dawn_native {

    void FingerprintRecorder::recordObject(RecordedObject* obj) {
        ASSERT(obj != nullptr);
        const size_t key = obj->getKey();
        if (key != kEmptyKeyValue) {
            record(key);
        } else {
            obj->fingerprint(this);
            obj->setKey(mHash);
        }
    }

    size_t FingerprintRecorder::getKey() const {
        ASSERT(mHash != kEmptyKeyValue);
        return mHash;
    }

    size_t RecordedObject::getKey() const {
        ASSERT(mKey != kEmptyKeyValue);
        return mKey;
    }

    void RecordedObject::setKey(size_t key) {
        ASSERT(mKey != kEmptyKeyValue);
        mKey = key;
    }

    size_t RecordedObject::HashFunc::operator()(const RecordedObject* obj) const {
        return obj->getKey();
    }
}  // namespace dawn_native
