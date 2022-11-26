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

#ifndef SRC_DAWN_NATIVE_EMULATOR_ADAPTEREMULATOR_H_
#define SRC_DAWN_NATIVE_EMULATOR_ADAPTEREMULATOR_H_

#include "dawn/native/Adapter.h"

namespace dawn::native::emulator {

class Adapter : public AdapterBase {
  public:
    explicit Adapter(InstanceBase* instance);
    ~Adapter() override;

    bool SupportsExternalImages() const override;

  private:
    MaybeError InitializeImpl() override;
    void InitializeSupportedFeaturesImpl() override;
    MaybeError InitializeSupportedLimitsImpl(CombinedLimits* limits) override;

    ResultOrError<Ref<DeviceBase>> CreateDeviceImpl(
        const DeviceDescriptor* descriptor,
        const TripleStateTogglesSet& userProvidedToggles) override;

    MaybeError ValidateFeatureSupportedWithTogglesImpl(
        wgpu::FeatureName feature,
        const TripleStateTogglesSet& userProvidedToggles) override;
};

}  // namespace dawn::native::emulator

#endif  // SRC_DAWN_NATIVE_EMULATOR_ADAPTEREMULATOR_H_
