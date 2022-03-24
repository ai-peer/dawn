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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <iomanip>
#include <string>

#include "dawn/tests/DawnNativeTest.h"

#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/CacheKey.h"
#include "dawn/native/utils/WGPUHelpers.h"

namespace dawn::native {

    // Testing classes with mock serializing implemented for testing.
    class A {
      public:
        MOCK_METHOD(void, SerializeMock, (CacheKey*, const A&), (const));
    };
    template <>
    void CacheKeySerializer<A>::Serialize(CacheKey* key, const A& t) {
        t.SerializeMock(key, t);
    }

    // Custom printer for CacheKey for clearer debug testing messages.
    void PrintTo(const CacheKey& key, std::ostream* stream) {
        *stream << std::hex;
        for (const int b : key) {
            *stream << std::setfill('0') << std::setw(2) << b << " ";
        }
        *stream << std::dec;
    }

    // Custom printer for CacheKey for clearer debug testing messages.
    void PrintTo(const CacheKey& key, std::ostream* stream) {
        *stream << std::string(key.begin(), key.end());
    }

    namespace {

        using ::testing::InSequence;
        using ::testing::NotNull;
        using ::testing::PrintToString;
        using ::testing::Ref;

        // Matcher to compare CacheKeys for easier testing.
        MATCHER_P(CacheKeyEq, key, PrintToString(key)) {
            return memcmp(arg.data(), key.data(), arg.size()) == 0;
        }

        TEST(CacheKeyGeneratorTests, RecordSingleMember) {
            CacheKey expected;
            SerializeInto(&expected, CacheKeyGenerator::MemberId(0));

            A a;
            EXPECT_CALL(a, SerializeMock(NotNull(), Ref(a))).Times(1);
            EXPECT_THAT(CacheKeyGenerator().Record(a).GetCacheKey(), CacheKeyEq(expected));
        }

        TEST(CacheKeyGeneratorTests, RecordManyMembers) {
            constexpr CacheKeyGenerator::MemberId kNumMembers = 100;

            CacheKey expected;
            CacheKeyGenerator gen;
            for (CacheKeyGenerator::MemberId id = 0; id < kNumMembers; id++) {
                A a;
                EXPECT_CALL(a, SerializeMock(NotNull(), Ref(a))).Times(1);
                gen.Record(a);

                // Generating the expected in the same loop.
                SerializeInto(&expected, id);
            }
            EXPECT_THAT(gen.GetCacheKey(), CacheKeyEq(expected));
        }

        TEST(CacheKeyGeneratorTests, RecordIterable) {
            constexpr size_t kIterableSize = 100;

            // Expecting member id followed by the size of the container.
            CacheKey expected;
            SerializeInto(&expected, CacheKeyGenerator::MemberId(0));
            SerializeInto(&expected, kIterableSize);

            std::vector<A> iterable(kIterableSize);
            {
                InSequence seq;
                for (const auto& a : iterable) {
                    EXPECT_CALL(a, SerializeMock(NotNull(), Ref(a))).Times(1);
                }
            }

            EXPECT_THAT(CacheKeyGenerator().RecordIterable(iterable).GetCacheKey(),
                        CacheKeyEq(expected));
        }

        TEST(CacheKeyGeneratorTests, RecordNested) {
            CacheKey expected;
            CacheKeyGenerator gen;
            {
                // Recording a single member.
                SerializeInto(&expected, CacheKeyGenerator::MemberId(0));

                A a;
                EXPECT_CALL(a, SerializeMock(NotNull(), Ref(a))).Times(1);
                CacheKeyGenerator(gen).Record(a);
                SerializeInto(&expected, CacheKeyGenerator::MemberId(0));
            }
            {
                // Recording multiple members.
                SerializeInto(&expected, CacheKeyGenerator::MemberId(1));

                constexpr CacheKeyGenerator::MemberId kNumMembers = 2;
                CacheKeyGenerator sub(gen);
                for (CacheKeyGenerator::MemberId id = 0; id < kNumMembers; id++) {
                    A a;
                    EXPECT_CALL(a, SerializeMock(NotNull(), Ref(a))).Times(1);
                    sub.Record(a);

                    // Generating the expected in the same loop.
                    SerializeInto(&expected, id);
                }
            }
            {
                // Record an iterable.
                SerializeInto(&expected, CacheKeyGenerator::MemberId(2));

                constexpr size_t kIterableSize = 2;
                std::vector<A> iterable(kIterableSize);
                {
                    InSequence seq;
                    for (const auto& a : iterable) {
                        EXPECT_CALL(a, SerializeMock(NotNull(), Ref(a))).Times(1);
                    }
                }
                SerializeInto(&expected, CacheKeyGenerator::MemberId(0));
                SerializeInto(&expected, kIterableSize);

                CacheKeyGenerator(gen).RecordIterable(iterable);
            }
            EXPECT_THAT(gen.GetCacheKey(), CacheKeyEq(expected));
        }

        TEST(CacheKeySerializerTests, IntegralTypes) {
            // Only testing explicitly sized types for simplicity, and using 0s for larger types to
            // avoid dealing with endianess.
            {
                CacheKey key;
                SerializeInto(&key, 'c');
                EXPECT_THAT(key, CacheKeyEq(CacheKey({'c'})));
            }
            {
                CacheKey key;
                SerializeInto(&key, (uint8_t)255);
                EXPECT_THAT(key, CacheKeyEq(CacheKey({255})));
            }
            {
                CacheKey key;
                SerializeInto(&key, (uint16_t)0);
                EXPECT_THAT(key, CacheKeyEq(CacheKey({0, 0})));
            }
            {
                CacheKey key;
                SerializeInto(&key, (uint32_t)0);
                EXPECT_THAT(key, CacheKeyEq(CacheKey({0, 0, 0, 0})));
            }
        }

        TEST(CacheKeySerializerTests, FloatingTypes) {
            // Using 0s to avoid dealing with implementation specific float details.
            {
                CacheKey key;
                SerializeInto(&key, (float)0);
                EXPECT_THAT(key, CacheKeyEq(CacheKey(sizeof(float), 0)));
            }
            {
                CacheKey key;
                SerializeInto(&key, (double)0);
                EXPECT_THAT(key, CacheKeyEq(CacheKey(sizeof(double), 0)));
            }
        }

        TEST(CacheKeySerializerTests, Strings) {
            std::string str = "string";

            CacheKey expected;
            SerializeInto(&expected, (size_t)6);
            expected.insert(expected.end(), str.begin(), str.end());

            {
                CacheKey key;
                SerializeInto(&key, "string");
                EXPECT_THAT(key, CacheKeyEq(expected));
            }
            {
                CacheKey key;
                SerializeInto(&key, str);
                EXPECT_THAT(key, CacheKeyEq(expected));
            }
        }

        TEST(CacheKeySerializerTests, CacheKeys) {
            CacheKey data = {'d', 'a', 't', 'a'};

            CacheKey expected;
            SerializeInto(&expected, (size_t)4);
            expected.insert(expected.end(), data.begin(), data.end());

            CacheKey key;
            SerializeInto(&key, data);
            EXPECT_THAT(key, CacheKeyEq(expected));
        }

        class CacheKeyDawnNativeTests : public DawnNativeTest {};

        TEST_F(CacheKeyDawnNativeTests, BindGroupLayouts) {
            // Bind group layout serialization breakdown:
            //     0: Pipeline compatibility token
            //     1: List/Map of sorted binding entries according to binding number
            //         0: Binding number
            //         1: Visibility        Vertex=1,Fragment=2,Compute=4
            //         2: Binding type      Buffer=0,Sampler=1,Texture=2,StorageTexture=3
            //         3: Buffer            Default: {0:0,1:0,2:0}
            //         4: Sampler           Default: {0:0}
            //         5: Texture           Default: {0:0,1:0,2:0}
            //         6: Storage Texture   Default: {0:0,1:0,2:0}
            {
                // Buffer binding type.
                Ref<BindGroupLayoutBase> bindGroupLayout;
                DAWN_ASSERT_AND_ASSIGN(
                    bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        dawn::native::FromAPI(device.Get()),
                        {{0, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform}}));
                EXPECT_THAT(bindGroupLayout->GetCacheKey(), CacheKeyEq(R"(
                    { 0:0,
                      1:[
                          { 0:0,
                            1:1,
                            2:0,
                            3:{0:1,1:0,2:0},
                            4:{0:0},
                            5:{0:0,1:0,2:0},
                            6:{0:0,1:0,2:0}
                          },
                        ]
                    })"));
            }
            {
                // Sampler binding type.
                Ref<BindGroupLayoutBase> bindGroupLayout;
                DAWN_ASSERT_AND_ASSIGN(
                    bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        dawn::native::FromAPI(device.Get()),
                        {{1, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering}}));
                EXPECT_THAT(bindGroupLayout->GetCacheKey(), CacheKeyEq(R"(
                    { 0:0,
                      1:[
                          { 0:1,
                            1:2,
                            2:1,
                            3:{0:0,1:0,2:0},
                            4:{0:1},
                            5:{0:0,1:0,2:0},
                            6:{0:0,1:0,2:0}
                          },
                        ]
                    })"));
            }
            {
                // Texture binding type.
                Ref<BindGroupLayoutBase> bindGroupLayout;
                DAWN_ASSERT_AND_ASSIGN(
                    bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        dawn::native::FromAPI(device.Get()),
                        {{2, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Float}}));
                EXPECT_THAT(bindGroupLayout->GetCacheKey(), CacheKeyEq(R"(
                    { 0:0,
                      1:[
                          { 0:2,
                            1:4,
                            2:2,
                            3:{0:0,1:0,2:0},
                            4:{0:0},
                            5:{0:1,1:2,2:0},
                            6:{0:0,1:0,2:0}
                          },
                        ]
                    })"));
            }
            {
                // Storage texture binding type.
                Ref<BindGroupLayoutBase> bindGroupLayout;
                DAWN_ASSERT_AND_ASSIGN(
                    bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        dawn::native::FromAPI(device.Get()),
                        {{3, wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute,
                          wgpu::StorageTextureAccess::WriteOnly, wgpu::TextureFormat::RGBA8Uint}}));
                EXPECT_THAT(bindGroupLayout->GetCacheKey(), CacheKeyEq(R"(
                    { 0:0,
                      1:[
                          { 0:3,
                            1:6,
                            2:3,
                            3:{0:0,1:0,2:0},
                            4:{0:0},
                            5:{0:0,1:0,2:0},
                            6:{0:1,1:21,2:2}
                          },
                        ]
                    })"));
            }
            {
                // Multiple binding types.
                Ref<BindGroupLayoutBase> bindGroupLayout;
                DAWN_ASSERT_AND_ASSIGN(
                    bindGroupLayout,
                    utils::MakeBindGroupLayout(
                        dawn::native::FromAPI(device.Get()),
                        {{0, wgpu::ShaderStage::Vertex, wgpu::BufferBindingType::Uniform},
                         {1, wgpu::ShaderStage::Fragment, wgpu::SamplerBindingType::Filtering},
                         {2, wgpu::ShaderStage::Compute, wgpu::TextureSampleType::Float},
                         {3, wgpu::ShaderStage::Fragment | wgpu::ShaderStage::Compute,
                          wgpu::StorageTextureAccess::WriteOnly, wgpu::TextureFormat::RGBA8Uint}}));
                EXPECT_THAT(bindGroupLayout->GetCacheKey(), CacheKeyEq(R"(
                    { 0:0,
                      1:[
                          { 0:0,
                            1:1,
                            2:0,
                            3:{0:1,1:0,2:0},
                            4:{0:0},
                            5:{0:0,1:0,2:0},
                            6:{0:0,1:0,2:0}
                          },
                          { 0:1,
                            1:2,
                            2:1,
                            3:{0:0,1:0,2:0},
                            4:{0:1},
                            5:{0:0,1:0,2:0},
                            6:{0:0,1:0,2:0}
                          },
                          { 0:2,
                            1:4,
                            2:2,
                            3:{0:0,1:0,2:0},
                            4:{0:0},
                            5:{0:1,1:2,2:0},
                            6:{0:0,1:0,2:0}
                          },
                          { 0:3,
                            1:6,
                            2:3,
                            3:{0:0,1:0,2:0},
                            4:{0:0},
                            5:{0:0,1:0,2:0},
                            6:{0:1,1:21,2:2}
                          },
                        ]
                    })"));
            }
        }

    }  // namespace

}  // namespace dawn::native
