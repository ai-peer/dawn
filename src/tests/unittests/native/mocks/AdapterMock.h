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

#include "dawn_native/Adapter.h"

#include <gmock/gmock.h>

namespace dawn_native {

    class AdapterMock : public AdapterBase {
      public:
        AdapterMock(wgpu::BackendType backend) : AdapterBase(nullptr, backend) {
        }
        MOCK_METHOD(bool, SupportsExternalImages, (), (const, override));
        MOCK_METHOD(ResultOrError<DeviceBase*>,
                    CreateDeviceImpl,
                    (const DeviceDescriptor*),
                    (override));
    };

}  // namespace dawn_native
