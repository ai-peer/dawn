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

#ifndef DAWNNATIVE_VULKAN_EXTERNALTEXTUREVK_H_
#define DAWNNATIVE_VULKAN_EXTERNALTEXTUREVK_H_

#include "dawn_native/ExternalTexture.h"

namespace dawn::native::vulkan {

    class Device;

    class ExternalTexture final : public ExternalTextureBase {
      public:
        static ResultOrError<Ref<ExternalTexture>> Create(
            DeviceBase* device,
            const ExternalTextureDescriptor* descriptor);

      protected:
        using ExternalTextureBase::ExternalTextureBase;

        MaybeError Initialize(DeviceBase* device,
                              const ExternalTextureDescriptor* descriptor) override;

      private:
        Ref<TextureBase> mDummyTexture;
    };

}  // namespace dawn::native::vulkan

#endif  // DAWNNATIVE_VULKAN_EXTERNALTEXTUREVK_H_