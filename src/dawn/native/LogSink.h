// Copyright 2022 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_LOGSINK_H_
#define SRC_DAWN_NATIVE_LOGSINK_H_

#include "dawn/common/RefCounted.h"
#include "dawn/native/Device.h"
#include "dawn/webgpu.h"

namespace dawn::native {

class DeviceBase;

// LogSink is a simple class which provides a log-only view of the device.
// It should not be extended to do more than logging, or provide direct
// access to the device.
class LogSink {
  public:
    explicit LogSink(DeviceBase* device);
    void Emit(const char* message);
    void Emit(WGPULoggingType loggingType, const char* message);

  private:
    Ref<DeviceBase> mDevice;
};

}  // namespace dawn::native

#endif  // SRC_DAWN_NATIVE_LOGSINK_H_
