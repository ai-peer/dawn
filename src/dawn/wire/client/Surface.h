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

#ifndef SRC_DAWN_WIRE_CLIENT_SURFACE_H_
#define SRC_DAWN_WIRE_CLIENT_SURFACE_H_

#include "dawn/wire/client/ObjectBase.h"

namespace dawn::wire::client {

class Device;

class Surface final : public ObjectBase {
  public:
    explicit Surface(const ObjectBaseParams& params);
    ~Surface() override;

    WGPUTextureUsage GetSupportedUsages(WGPUAdapter);  // Not implemented in the wire.
};

}  // namespace dawn::wire::client

#endif
