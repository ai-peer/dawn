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

#include <gtest/gtest.h>

#include "dawn_native/Instance.h"
#include "dawn_native/null/DeviceNull.h"

class ExtensionTests : public testing::Test {
  public:
    ExtensionTests() : testing::Test(), instanceBase(), adapterBase(&instanceBase) {
    }

    dawn_native::Adapter CreateAdapterWithGivenExtensions(
        dawn_native::Extensions requiredExtensions) {
        adapterBase.SetSupportedExtensions(requiredExtensions);
        return dawn_native::Adapter(&adapterBase);
    }

  protected:
    dawn_native::InstanceBase instanceBase;
    dawn_native::null::Adapter adapterBase;
};

// Test we will fail to create a device when we request the extension textureCompressionBC on an
// adapter which does not support this extension.
TEST_F(ExtensionTests, AdapterWithBCFormatExtensionDisabled) {
    dawn_native::Extensions extensionWithoutBCFormat;
    extensionWithoutBCFormat.textureCompressionBC = false;
    dawn_native::Adapter adapterWithoutExtension =
        CreateAdapterWithGivenExtensions(extensionWithoutBCFormat);

    dawn_native::DeviceDescriptor deviceDescriptor;
    deviceDescriptor.requiredExtensions.textureCompressionBC = true;
    DawnDevice deviceWithExtension = adapterWithoutExtension.CreateDevice(&deviceDescriptor);
    ASSERT_EQ(nullptr, deviceWithExtension);
}
