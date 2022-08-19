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

#include "dawn/native/Error.h"
#include "dawn/native/stream/Stream.h"

#import <Metal/Metal.h>

namespace dawn::native {

template <>
MaybeError stream::Stream<MTLSize>::Read(stream::Source* s, MTLSize* v) {
    DAWN_TRY(StreamOut(s, &v->width, &v->height, &v->depth));
    return {};
}

template <>
void stream::Stream<MTLSize>::Write(stream::Sink* s, const MTLSize& v) {
    StreamIn(s, v.width, v.height, v.depth);
}

}  // namespace dawn::native
