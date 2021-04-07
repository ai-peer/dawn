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

#ifndef COMMON_CACHEDDATA_H_
#define COMMON_CACHEDDATA_H_

#include "common/RefCounted.h"
#include "dawn_platform/DawnPlatform.h"

class CachedData : public dawn_platform::CachedBlob, RefCounted {
  public:
    CachedData(const uint8_t* data, size_t size);

    const uint8_t* data() override;
    size_t size() const override;

    void Reference() override;
    virtual void Release() override;

  private:
    std::unique_ptr<uint8_t[]> buffer;
    size_t bufferSize = 0;
};

#endif  // COMMON_CACHEDDATA_H_
