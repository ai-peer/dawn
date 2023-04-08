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

#include <algorithm>

#include "dawn/native/d3d/D3DError.h"
#include "dawn/native/d3d11/DeviceD3D11.h"
#include "dawn/native/d3d11/UtilsD3D11.h"

namespace dawn::native::d3d11 {

namespace {
D3D11_QUERY D3D11QueryType(wgpu::QueryType type) {
    switch (type) {
        case wgpu::QueryType::Occlusion:
            return D3D11_QUERY_OCCLUSION;
        case wgpu::QueryType::PipelineStatistics:
            return D3D11_QUERY_PIPELINE_STATISTICS;
        case wgpu::QueryType::Timestamp:
            return D3D11_QUERY_TIMESTAMP;
        default:
            UNREACHABLE();
    }
}
}  // anonymous namespace

// static
ResultOrError<Ref<QuerySet>> QuerySet::Create(Device* device,
                                              const QuerySetDescriptor* descriptor) {
    Ref<QuerySet> querySet = AcquireRef(new QuerySet(device, descriptor));
    DAWN_TRY(querySet->Initialize());
    return querySet;
}

MaybeError QuerySet::Initialize() {
    D3D11_QUERY_DESC queryDesc;
    queryDesc.Query = D3D11QueryType(GetQueryType());
    queryDesc.MiscFlags = 0;

    ID3D11Device* d3d11Device = ToBackend(GetDevice())->GetD3D11Device();
    DAWN_TRY(CheckOutOfMemoryHRESULT(d3d11Device->CreateQuery(&queryDesc, &mD3d11Query),
                                     "ID3D11Device::CreateQuery"));

    SetLabelImpl();

    return {};
}

ID3D11Query* QuerySet::GetD3D11Query() const {
    return mD3d11Query.Get();
}

QuerySet::~QuerySet() = default;

void QuerySet::DestroyImpl() {
    QuerySetBase::DestroyImpl();
    mD3d11Query = nullptr;
}

void QuerySet::SetLabelImpl() {
    SetDebugName(ToBackend(GetDevice()), mD3d11Query.Get(), "Dawn_QuerySet", GetLabel());
}

}  // namespace dawn::native::d3d11
