// Copyright 2017 The Dawn Authors
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

#include "dawn_native/QuerySet.h"

#include "dawn_native/Device.h"
#include "dawn_native/Extensions.h"

namespace dawn_native {

    MaybeError ValidateQuerySetDescriptor(DeviceBase* device,
                                          const QuerySetDescriptor* descriptor) {
        if (descriptor->nextInChain != nullptr) {
            return DAWN_VALIDATION_ERROR("nextInChain must be nullptr");
        }

        if (descriptor->count <= 0) {
            return DAWN_VALIDATION_ERROR("The count of query set must be greater than 0");
        }

        if (descriptor->type == wgpu::QueryType::PipelineStatistics &&
            !device->IsExtensionEnabled(Extension::PipelineStatisticsQuery)) {
            return DAWN_VALIDATION_ERROR("The pipeline statistics query feature is not supported");
        }

        if (descriptor->type == wgpu::QueryType::Timestamp &&
            !device->IsExtensionEnabled(Extension::TimestampQuery)) {
            return DAWN_VALIDATION_ERROR("The timestamp query feature is not supported");
        }

        // Todo(hao.x.li): Add validation for pipelineStatistics.

        return {};
    }

    QuerySetBase::QuerySetBase(DeviceBase* device, const QuerySetDescriptor* descriptor)
        : ObjectBase(device),
          mQueryType(descriptor->type),
          mQueryCount(descriptor->count),
          mState(QueryState::Available) {
    }

    QuerySetBase::QuerySetBase(DeviceBase* device, ObjectBase::ErrorTag tag)
        : ObjectBase(device, tag), mState(QueryState::Unavailable) {
    }

    QuerySetBase::~QuerySetBase() {
    }

    // static
    QuerySetBase* QuerySetBase::MakeError(DeviceBase* device) {
        return new QuerySetBase(device, ObjectBase::kError);
    }

    wgpu::QueryType QuerySetBase::GetQueryType() {
        return mQueryType;
    }

    uint32_t QuerySetBase::GetQueryCount() {
        return mQueryCount;
    }

    void QuerySetBase::DestroyImpl() {
        UNREACHABLE();
    }

    void QuerySetBase::Destroy() {
        if (GetDevice()->ConsumedError(ValidateDestroy())) {
            return;
        }
        DestroyInternal();
    }

    MaybeError QuerySetBase::ValidateDestroy() const {
        DAWN_TRY(GetDevice()->ValidateObject(this));
        return {};
    }

    void QuerySetBase::DestroyInternal() {
        if (mState != QueryState::Destroyed) {
            DestroyImpl();
        }
        mState = QueryState::Destroyed;
    }

}  // namespace dawn_native
