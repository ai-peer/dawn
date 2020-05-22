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

#include "dawn_native/vulkan/QuerySetVk.h"

#include "dawn_native/vulkan/DeviceVk.h"
#include "dawn_native/vulkan/FencedDeleter.h"
#include "dawn_native/vulkan/VulkanError.h"
#include "dawn_platform/DawnPlatform.h"

namespace dawn_native { namespace vulkan {

    // static
    ResultOrError<QuerySet*> QuerySet::Create(Device* device,
                                              const QuerySetDescriptor* descriptor) {
        Ref<QuerySet> queryset = AcquireRef(new QuerySet(device, descriptor));
        DAWN_TRY(queryset->Initialize());
        return queryset.Detach();
    }

    MaybeError QuerySet::Initialize() {
        VkQueryPoolCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        switch (GetQueryType()) {
            case wgpu::QueryType::Occlusion:
                createInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
                break;
            case wgpu::QueryType::PipelineStatistics:
                createInfo.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
                createInfo.pipelineStatistics =
                    VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
                    VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
                break;
            case wgpu::QueryType::Timestamp:
                createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
                break;
            default:
                UNREACHABLE();
                break;
        }
        createInfo.queryCount = GetQueryCount();

        Device* device = ToBackend(GetDevice());
        return CheckVkSuccess(
            device->fn.CreateQueryPool(device->GetVkDevice(), &createInfo, nullptr, &*mHandle),
            "vkCreateQueryPool");
    }

    VkQueryPool QuerySet::GetHandle() const {
        return mHandle;
    }

    QuerySet::~QuerySet() {
        DestroyInternal();
    }

    void QuerySet::DestroyImpl() {
        if (mHandle != VK_NULL_HANDLE) {
            ToBackend(GetDevice())->GetFencedDeleter()->DeleteWhenUnused(mHandle);
            mHandle = VK_NULL_HANDLE;
        }
    }

}}  // namespace dawn_native::vulkan
