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

#include "dawn/native/emulator/TextureEmulator.h"

#include "dawn/native/ErrorData.h"
#include "dawn/native/emulator/DeviceEmulator.h"

namespace dawn::native::emulator {

// static
ResultOrError<Ref<Texture>> Texture::Create(Device* device, const TextureDescriptor* descriptor) {
    Ref<Texture> texture =
        AcquireRef(new Texture(device, descriptor, TextureBase::TextureState::OwnedInternal));
    DAWN_TRY(texture->Initialize());
    return std::move(texture);
}

Texture::Texture(Device* device, const TextureDescriptor* descriptor, TextureState state)
    : TextureBase(device, descriptor, state) {}

Texture::~Texture() {}

MaybeError Texture::Initialize() {
    // TODO: size, format, etc
    mTexture = std::make_unique<tint::interp::Texture>();
    return {};
}

// static
ResultOrError<Ref<TextureView>> TextureView::Create(TextureBase* texture,
                                                    const TextureViewDescriptor* descriptor) {
    Ref<TextureView> view = AcquireRef(new TextureView(texture, descriptor));
    DAWN_TRY(view->Initialize());
    return view;
}

TextureView::~TextureView() {}

MaybeError TextureView::Initialize() {
    // TODO: create from the texture object
    mTextureView = std::make_unique<tint::interp::TextureView>();
    return {};
}

}  // namespace dawn::native::emulator
