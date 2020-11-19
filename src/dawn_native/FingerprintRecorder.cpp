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

#include "common/Assert.h"
#include "dawn_native/CachedObject.h"

namespace dawn_native {

    void FingerprintRecorder::RecordObject(CachedObject* obj) {
        ASSERT(obj != nullptr);
        // Do not record the object again if previously recorded.
        // The cached object hash is produced once and never changes for the life of the
        // object. If another object must record this object, it cannot re-record since it
        // would produce a new hash from the original hash which is not allowed.
        if (obj->mIsHashInitialized) {
            Record(obj->mHash);
        } else {
            obj->Fingerprint(this);
            obj->mHash = mHash;
            obj->mIsHashInitialized = true;
        }
    }
}  // namespace dawn_native
