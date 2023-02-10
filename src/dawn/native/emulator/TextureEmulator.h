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

#ifndef SRC_DAWN_NATIVE_EMULATOR_TEXTUREEMULATOR_H_
#define SRC_DAWN_NATIVE_EMULATOR_TEXTUREEMULATOR_H_

#include "dawn/native/Texture.h"

#include <memory>

#include "tint/interp/texture.h"

namespace dawn::native::emulator {

class Device;

class Texture final : public TextureBase {
  public:
    // Used to create a regular texture from a descriptor.
    static ResultOrError<Ref<Texture>> Create(Device* device, const TextureDescriptor* descriptor);

    tint::interp::Texture& Get() const { return *mTexture; }

  private:
    Texture(Device* device, const TextureDescriptor* descriptor, TextureState state);
    ~Texture() override;
    using TextureBase::TextureBase;
    MaybeError Initialize();

    std::unique_ptr<tint::interp::Texture> mTexture;
};

class TextureView final : public TextureViewBase {
  public:
    static ResultOrError<Ref<TextureView>> Create(TextureBase* texture,
                                                  const TextureViewDescriptor* descriptor);
    tint::interp::TextureView& Get() const { return *mTextureView; }

  private:
    ~TextureView() override;
    using TextureViewBase::TextureViewBase;
    MaybeError Initialize();

    std::unique_ptr<tint::interp::TextureView> mTextureView;
};

}  // namespace dawn::native::emulator

#endif  // SRC_DAWN_NATIVE_EMULATOR_TEXTUREEMULATOR_H_
