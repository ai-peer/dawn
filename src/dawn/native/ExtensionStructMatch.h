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

#ifndef SRC_DAWN_NATIVE_EXTENSIONSTRUCTMATCH_H_
#define SRC_DAWN_NATIVE_EXTENSIONSTRUCTMATCH_H_

#include "dawn/native/ChainUtils_autogen.h"
#include "dawn/native/Device.h"
#include "dawn/native/Features.h"

namespace dawn::native {

namespace detail {

template <typename DeviceType, typename BaseDescriptor, typename Case, typename... Cases>
std::
    invoke_result_t<typename Case::Call, DeviceType*, const char*, const typename Case::Descriptor*>
    ExtensionStructMatchImpl(DeviceType* device,
                             const BaseDescriptor* baseDescriptor,
                             Case c,
                             Cases&&... cases) {
    const typename Case::Descriptor* descriptor = nullptr;
    FindInChain(baseDescriptor->nextInChain, &descriptor);
    if (descriptor != nullptr) {
        DAWN_INVALID_IF(!device->HasFeature(Case::kFeature), "%s is not enabled.",
                        FeatureEnumToAPIFeature(Case::kFeature));
        return c.call(device, baseDescriptor->label, descriptor);
    }
    if constexpr (sizeof...(cases) > 0) {
        return TryCaseDispatch(device, baseDescriptor, std::forward<Case>(cases)...);
    } else {
        UNREACHABLE();
    }
}
}  // namespace detail

template <typename DescriptorType, Feature FeatureEnum, typename CallFn>
struct ExtensionStructCaseImpl {
    static constexpr Feature kFeature = FeatureEnum;
    using Descriptor = DescriptorType;
    using Call = CallFn;

    explicit ExtensionStructCaseImpl(CallFn call) : call(call) {}
    CallFn call;
};

template <typename DescriptorType, Feature FeatureEnum, typename CallFn>
auto ExtensionStructCase(CallFn createFn) {
    return ExtensionStructCaseImpl<DescriptorType, FeatureEnum, CallFn>(createFn);
}

template <typename DeviceType, typename BaseDescriptor, typename Case, typename... Cases>
std::
    invoke_result_t<typename Case::Call, DeviceType*, const char*, const typename Case::Descriptor*>
    ExtensionStructMatch(DeviceType* device,
                         const BaseDescriptor* baseDescriptor,
                         Case&& c,
                         Cases&&... cases) {
    DAWN_INVALID_IF(baseDescriptor->nextInChain == nullptr, "Chained extension struct required.");
    DAWN_TRY(ValidateSingleSType(baseDescriptor->nextInChain, STypeFor<typename Case::Descriptor>,
                                 STypeFor<typename Cases::Descriptor>...));
    return detail::ExtensionStructMatchImpl(device, baseDescriptor, std::move(c),
                                            std::forward<Case>(cases)...);
}

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_EXTENSIONSTRUCTMATCH_H_
