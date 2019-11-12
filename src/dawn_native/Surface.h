// Copyright 2019 The Dawn Authors
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

#ifndef DAWNNATIVE_SURFACE_H_
#define DAWNNATIVE_SURFACE_H_

#include "dawn_native/Error.h"
#include "dawn_native/Forward.h"
#include "dawn_native/RefCounted.h"

#include "dawn_native/dawn_platform.h"

namespace dawn_native {

    MaybeError ValidateSwapChainDescriptor(const InstanceBase* instance,
                                           const SwapChainDescriptor* descriptor);

    enum class SurfaceType {
        MetalLayer,
        WindowsHwnd,
        Xlib
    };

    class Surface final : public RefCounted {
        public:
            Surface(InstanceBase* instance, const SwapChainDescriptor* descriptor);
        
        private:
            SurfaceType mType;


    };

}  // namespace dawn_native

#endif  // DAWNNATIVE_SURFACE_H_
