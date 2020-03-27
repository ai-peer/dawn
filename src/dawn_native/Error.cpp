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

#include "dawn_native/Error.h"

#include "dawn_native/ErrorData.h"
#include "dawn_native/dawn_platform.h"

#include "common/Log.h"

namespace dawn_native {

    void AssertAndIgnoreDeviceLossError(MaybeError maybeError) {
        if (maybeError.IsError()) {
            std::unique_ptr<ErrorData> errorData = maybeError.AcquireError();

            for (auto& record : errorData->GetBacktrace()) {
                DAWN_DEBUG() << "  - " << record.file << ":" << record.line << "("
                             << record.function << ")";
            }

            ASSERT(errorData->GetType() == InternalErrorType::DeviceLost);
        }
    }

    wgpu::ErrorType ToWGPUErrorType(InternalErrorType type) {
        switch (type) {
            case InternalErrorType::Validation:
                return wgpu::ErrorType::Validation;
            case InternalErrorType::OutOfMemory:
                return wgpu::ErrorType::OutOfMemory;
            case InternalErrorType::DeviceLost:
            case InternalErrorType::Internal:
                return wgpu::ErrorType::DeviceLost;
            default:
                return wgpu::ErrorType::Unknown;
        }
    }

    InternalErrorType FromWGPUErrorType(wgpu::ErrorType type) {
        switch (type) {
            case wgpu::ErrorType::Validation:
                return InternalErrorType::Validation;
            case wgpu::ErrorType::OutOfMemory:
                return InternalErrorType::OutOfMemory;
            case wgpu::ErrorType::DeviceLost:
            default:
                return InternalErrorType::Internal;
        }
    }

}  // namespace dawn_native
