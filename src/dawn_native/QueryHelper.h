// Copyright 2020 The Dawn Authors
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

#ifndef DAWNNATIVE_QUERYHELPER_H_
#define DAWNNATIVE_QUERYHELPER_H_

#include "dawn_native/Error.h"
#include "dawn_native/ObjectBase.h"

namespace dawn::native {

    class BufferBase;
    class CommandEncoder;

    struct TimestampParams {
        uint32_t first;
        uint32_t count;
        uint32_t offset;
        // To improve the precision of the product of timestamp and period in the post-processing
        // compute shader, we simulate the multiplication by unsigned 32-bit integers.
        // Here the period is obtained by multiplying the origin period (float, got from GPUDevice)
        // by a factor and converting it to an unsigned 32-bit integer.
        uint32_t period;
        // Must be a power of 2, used to multiply the origin period (float) and do division using
        // right shifting in the post-processing compute shader.
        uint32_t factor;
    };

    MaybeError EncodeConvertTimestampsToNanoseconds(CommandEncoder* encoder,
                                                    BufferBase* timestamps,
                                                    BufferBase* availability,
                                                    BufferBase* params);

}  // namespace dawn::native

#endif  // DAWNNATIVE_QUERYHELPER_H_
