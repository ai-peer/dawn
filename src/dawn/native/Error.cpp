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

#include "dawn/native/Error.h"

#include "dawn/native/ErrorData.h"
#include "dawn/native/dawn_platform.h"

namespace dawn::native {

void IgnoreErrors(MaybeError maybeError) {
    if (maybeError.IsError()) {
        std::unique_ptr<ErrorData> errorData = maybeError.AcquireError();
        // During shutdown and destruction, device lost errors can be ignored.
        // We can also ignore other unexpected internal errors on shut down and treat it as
        // device lost so that we can continue with destruction.
        ASSERT(errorData->GetType() == DawnErrorType::DeviceLost ||
               errorData->GetType() == DawnErrorType::Internal);
    }
}

wgpu::ErrorType ToWGPUErrorType(DawnErrorType type) {
    switch (type) {
        case DawnErrorType::Validation:
            return wgpu::ErrorType::Validation;
        case DawnErrorType::OutOfMemory:
            return wgpu::ErrorType::OutOfMemory;

        // There is no equivalent of Internal errors in the WebGPU API. Internal errors cause
        // the device at the API level to be lost, so treat it like a DeviceLost error.
        case DawnErrorType::Internal:
        case DawnErrorType::DeviceLost:
            return wgpu::ErrorType::DeviceLost;

        default:
            return wgpu::ErrorType::Unknown;
    }
}

DawnErrorType FromWGPUErrorType(wgpu::ErrorType type) {
    switch (type) {
        case wgpu::ErrorType::Validation:
            return DawnErrorType::Validation;
        case wgpu::ErrorType::OutOfMemory:
            return DawnErrorType::OutOfMemory;
        case wgpu::ErrorType::DeviceLost:
            return DawnErrorType::DeviceLost;
        default:
            return DawnErrorType::Internal;
    }
}

absl::FormatConvertResult<absl::FormatConversionCharSet::kString |
                          absl::FormatConversionCharSet::kIntegral>
AbslFormatConvert(DawnErrorType value,
                  const absl::FormatConversionSpec& spec,
                  absl::FormatSink* s) {
    if (spec.conversion_char() == absl::FormatConversionChar::s) {
        if (!static_cast<bool>(value)) {
            s->Append("None");
            return {true};
        }

        bool moreThanOneBit = !HasZeroOrOneBits(value);
        if (moreThanOneBit) {
            s->Append("(");
        }

        bool first = true;
        if (value & DawnErrorType::Validation) {
            if (!first) {
                s->Append("|");
            }
            first = false;
            s->Append("Validation");
            value &= ~DawnErrorType::Validation;
        }
        if (value & DawnErrorType::DeviceLost) {
            if (!first) {
                s->Append("|");
            }
            first = false;
            s->Append("DeviceLost");
            value &= ~DawnErrorType::DeviceLost;
        }
        if (value & DawnErrorType::Internal) {
            if (!first) {
                s->Append("|");
            }
            first = false;
            s->Append("Internal");
            value &= ~DawnErrorType::Internal;
        }
        if (value & DawnErrorType::OutOfMemory) {
            if (!first) {
                s->Append("|");
            }
            first = false;
            s->Append("OutOfMemory");
            value &= ~DawnErrorType::OutOfMemory;
        }

        if (moreThanOneBit) {
            s->Append(")");
        }
    } else {
        s->Append(absl::StrFormat(
            "%u", static_cast<typename std::underlying_type<DawnErrorType>::type>(value)));
    }
    return {true};
}

}  // namespace dawn::native
