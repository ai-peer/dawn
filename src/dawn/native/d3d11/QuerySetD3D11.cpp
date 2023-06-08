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

#include "dawn/native/d3d11/QuerySetD3D11.h"

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/CommandBufferD3D11.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"
#include "dawn/utils/SystemUtils.h"

namespace dawn::native::d3d11 {

ResultOrError<Ref<QuerySet>> QuerySet::Create(Device* device,
                                              const QuerySetDescriptor* descriptor) {
    Ref<QuerySet> querySet = AcquireRef(new QuerySet(device, descriptor));
    DAWN_TRY(querySet->Initialize());
    return querySet;
}

MaybeError QuerySet::CreateD3D11OcclusionQuery(ID3D11Query** query) {
    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_OCCLUSION;
    DAWN_TRY(CheckHRESULT(ToBackend(GetDevice())->GetD3D11Device()->CreateQuery(&queryDesc, query),
                          "ID3D11Device::CreateQuery"));
    return {};
}

MaybeError QuerySet::Initialize() {
    ASSERT(GetQueryType() == wgpu::QueryType::Occlusion);
    for (uint32_t i = 0; i < GetQueryCount(); ++i) {
        ComPtr<ID3D11Query> d3d11Query;
        DAWN_TRY(CreateD3D11OcclusionQuery(&d3d11Query));
        mQuerySet.push_back(std::move(d3d11Query));
        mQueryState.push_back(State::Begin);
    }
    SetLabelImpl();
    return {};
}

void QuerySet::DestroyImpl() {
    QuerySetBase::DestroyImpl();
    mQuerySet.clear();
    mQueryState.clear();
}

void QuerySet::SetLabelImpl() {
    for (const auto& query : mQuerySet) {
        SetDebugName(ToBackend(GetDevice()), query.Get(), "Dawn_QuerySet", GetLabel());
    }
}

void QuerySet::BeginQuery(ID3D11DeviceContext* d3d11DeviceContext, uint32_t query) {
    // Don't reuse the existing d3d11 qeury. The debug layer might complain:
    //  "Begin is being invoked on a Query, where the previous results have not been obtained with
    //  GetData."
    if (mQueryState[query] != State::Begin) {
        IgnoreErrors(CreateD3D11OcclusionQuery(&mQuerySet[query]));
    }
    d3d11DeviceContext->Begin(mQuerySet[query].Get());
    mQueryState[query] = State::End;
}

void QuerySet::EndQuery(ID3D11DeviceContext* d3d11DeviceContext, uint32_t query) {
    d3d11DeviceContext->End(mQuerySet[query].Get());
    mQueryState[query] = State::GetData;
}

MaybeError QuerySet::Resolve(ID3D11DeviceContext* d3d11DeviceContext,
                             uint32_t firstQuery,
                             uint32_t queryCount,
                             uint8_t* destination) {
    for (uint32_t i = firstQuery; i < firstQuery + queryCount; ++i) {
        if (mQueryState[i] == State::GetData) {
            // Check if the data is already available.
            HRESULT hr = d3d11DeviceContext->GetData(mQuerySet[i].Get(), nullptr, 0, 0u);
            while (hr == S_FALSE) {
                // It's a pity we have to wait until GPU is ready for the data.
                // TODO(dawn:1705): Get the query data asynchronously.
                utils::USleep(100);
                hr = d3d11DeviceContext->GetData(mQuerySet[i].Get(), nullptr, 0, 0u);
            }
            DAWN_TRY(CheckHRESULT(
                d3d11DeviceContext->GetData(mQuerySet[i].Get(), destination, sizeof(uint64_t), 0u),
                "ID3D11DeviceContext::GetData"));
        } else {
            // Resolve to zero if the query didn't begin and end.
            uint64_t zero = 0u;
            std::memcpy(destination, &zero, sizeof(zero));
        }
        destination += sizeof(uint64_t);
    }
    return {};
}

}  // namespace dawn::native::d3d11
