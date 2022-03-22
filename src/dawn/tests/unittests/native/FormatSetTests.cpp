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

#include "dawn/tests/DawnNativeTest.h"

#include "dawn/native/Device.h"
#include "dawn/native/Format.h"

namespace {
    using namespace dawn::native;

    class FormatSetTests : public DawnNativeTest {};

    // Test that bits in the FormatSet may be set and checked.
    TEST_F(FormatSetTests, SetAndCheck) {
        DeviceBase* deviceBase = reinterpret_cast<DeviceBase*>(device.Get());

        const Format& f1 = deviceBase->GetValidInternalFormat(wgpu::TextureFormat::BGRA8UnormSrgb);
        const Format& f2 = deviceBase->GetValidInternalFormat(wgpu::TextureFormat::RG8Snorm);
        const Format& f3 = deviceBase->GetValidInternalFormat(wgpu::TextureFormat::RGBA16Float);

        FormatSet formatSet;
        // Initially, all false.
        EXPECT_FALSE(formatSet.Has(f1));
        EXPECT_FALSE(formatSet.Has(f2));
        EXPECT_FALSE(formatSet.Has(f3));
        EXPECT_FALSE(formatSet.Any());

        // Set format 1, not the others.
        formatSet.Set(f1);
        EXPECT_TRUE(formatSet.Has(f1));
        EXPECT_FALSE(formatSet.Has(f2));
        EXPECT_FALSE(formatSet.Has(f3));
        EXPECT_TRUE(formatSet.Any());

        // Also test setting format 3
        formatSet.Set(f3);
        EXPECT_TRUE(formatSet.Has(f1));
        EXPECT_FALSE(formatSet.Has(f2));
        EXPECT_TRUE(formatSet.Has(f3));
        EXPECT_TRUE(formatSet.Any());

        // Test un-setting values.
        formatSet.Set(f3, false);
        EXPECT_TRUE(formatSet.Has(f1));
        EXPECT_FALSE(formatSet.Has(f2));
        EXPECT_FALSE(formatSet.Has(f3));
        EXPECT_TRUE(formatSet.Any());

        formatSet.Set(f1, false);
        EXPECT_FALSE(formatSet.Has(f1));
        EXPECT_FALSE(formatSet.Has(f2));
        EXPECT_FALSE(formatSet.Has(f3));
        EXPECT_FALSE(formatSet.Any());
    }

    // Test that Formats in the FormatSet may be iterated in order.
    TEST_F(FormatSetTests, Iteration) {
        DeviceBase* deviceBase = reinterpret_cast<DeviceBase*>(device.Get());

        const Format& f1 = deviceBase->GetValidInternalFormat(wgpu::TextureFormat::BGRA8UnormSrgb);
        const Format& f2 = deviceBase->GetValidInternalFormat(wgpu::TextureFormat::RG8Snorm);
        const Format& f3 = deviceBase->GetValidInternalFormat(wgpu::TextureFormat::RGBA16Float);

        FormatSet formatSet;

        for (const Format& f : deviceBase->IterateFormatSet(formatSet)) {
            // Shouldn't iterate at all.
            ADD_FAILURE() << "Unexpected format index " << f.GetIndex();
        }

        // Set two formats.
        formatSet.Set(f2);
        formatSet.Set(f3);

        // Iterate and expect only the two formats in increasing index value.
        {
            uint32_t i = 0;
            for (const Format& f : deviceBase->IterateFormatSet(formatSet)) {
                switch (i++) {
                    case 0:
                        EXPECT_EQ(&f, &f2);
                        break;
                    case 1:
                        EXPECT_EQ(&f, &f3);
                        break;
                    default:
                        ADD_FAILURE() << "Unexpected format index " << f.GetIndex();
                        break;
                }
            }
        }

        // Set the remaining format.
        formatSet.Set(f1);

        // Iterate and expect all three formats in increasing index value.
        {
            uint32_t i = 0;
            for (const Format& f : deviceBase->IterateFormatSet(formatSet)) {
                switch (i++) {
                    case 0:
                        EXPECT_EQ(&f, &f2);
                        break;
                    case 1:
                        EXPECT_EQ(&f, &f1);
                        break;
                    case 2:
                        EXPECT_EQ(&f, &f3);
                        break;
                }
            }
        }
    }

}  // anonymous namespace