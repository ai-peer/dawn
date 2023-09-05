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

#include "dawn/tests/DawnTest.h"

namespace dawn {
namespace {

class PixelLocalStorageTests : public DawnTest {
  protected:
    void SetUp() override {
        DawnTest::SetUp();
        DAWN_TEST_UNSUPPORTED_IF(
            !device.HasFeature(wgpu::FeatureName::PixelLocalStorageCoherent) &&
            !device.HasFeature(wgpu::FeatureName::PixelLocalStorageNonCoherent));
    }

    std::vector<wgpu::FeatureName> GetRequiredFeatures() override {
        std::vector<wgpu::FeatureName> requiredFeatures = {};
        if (SupportsFeatures({wgpu::FeatureName::PixelLocalStorageCoherent})) {
            requiredFeatures.push_back(wgpu::FeatureName::PixelLocalStorageCoherent);
        }
        if (SupportsFeatures({wgpu::FeatureName::PixelLocalStorageNonCoherent})) {
            requiredFeatures.push_back(wgpu::FeatureName::PixelLocalStorageNonCoherent);
        }
        return requiredFeatures;
    }
};

TEST_P(PixelLocalStorageTests, Foo) {}

// TEST PLAN TIME!
//

DAWN_INSTANTIATE_TEST(PixelLocalStorageTests, MetalBackend());

}  // anonymous namespace
}  // namespace dawn
