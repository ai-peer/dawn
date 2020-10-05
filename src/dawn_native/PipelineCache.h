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

#ifndef DAWNNATIVE_PIPELINECACHE_H_
#define DAWNNATIVE_PIPELINECACHE_H_

#include "dawn_native/Error.h"
#include "dawn_native/PersistentCache.h"

namespace dawn_native {

    class DeviceBase;

    class PipelineCacheBase {
      public:
        PipelineCacheBase(DeviceBase* device);
        virtual ~PipelineCacheBase() = default;

        virtual MaybeError loadPipelineCacheIfNecessary() = 0;
        virtual MaybeError storePipelineCache() = 0;

      protected:
        std::string GetMetadataForKey() const;

        DeviceBase* mDeviceBase = nullptr;
        bool mIsPipelineCacheLoaded = false;
        PersistentCacheKey mPipelineCacheKey;
    };
}  // namespace dawn_native

#endif  // DAWNNATIVE_PIPELINECACHE_H_