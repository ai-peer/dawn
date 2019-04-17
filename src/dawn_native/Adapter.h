// Copyright 2018 The Dawn Authors
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

#ifndef DAWNNATIVE_ADAPTER_H_
#define DAWNNATIVE_ADAPTER_H_

#include "dawn_native/DawnNative.h"

#include "dawn_native/Error.h"

#include "platform/Workarounds.h"

namespace dawn_native {

    class DeviceBase;

    class AdapterBase {
      public:
        AdapterBase(InstanceBase* instance, BackendType backend);
        virtual ~AdapterBase() = default;

        BackendType GetBackendType() const;
        const PCIInfo& GetPCIInfo() const;
        InstanceBase* GetInstance() const;

        // Create device with optional workarounds. When the bit of the workaround is set in
        // the parameter appliedWorkaroundsMask, whether the workaround is enabled or not will
        // depend on the bit in the parameter workaroundsMask. Other workarounds won't be affected.
        DeviceBase* CreateDevice(const WorkaroundsMask* workaroundsMask = nullptr,
                                 const WorkaroundsMask* appliedWorkaroundsMask = nullptr);

      protected:
        PCIInfo mPCIInfo = {};

      private:
        virtual ResultOrError<DeviceBase*> CreateDeviceImpl(
            const WorkaroundsMask* workarounds,
            const WorkaroundsMask* appliedWorkaroundsMask) = 0;

        MaybeError CreateDeviceInternal(DeviceBase** result,
                                        const WorkaroundsMask* workarounds,
                                        const WorkaroundsMask* appliedWorkaroundsMask);

        InstanceBase* mInstance = nullptr;
        BackendType mBackend;
    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_ADAPTER_H_
