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

#include <algorithm>
#include <string>

#include "dawn/tests/DawnNativeTest.h"

#include "dawn/native/BindGroupLayout.h"
#include "dawn/native/CacheKey.h"
#include "dawn/native/utils/WGPUHelpers.h"

namespace dawn::native {

    // Testing classes/structs with serializing implemented for testing.
    struct A {};
    void SerializeInto(CacheKey* key, const A& t) {
        std::string str = "structA";
        key->insert(key->end(), str.begin(), str.end());
    }

    class B {};
    void SerializeInto(CacheKey* key, const B& t) {
        std::string str = "classB";
        key->insert(key->end(), str.begin(), str.end());
    }

    // Custom printer for CacheKey for clearer debug testing messages.
    void PrintTo(const CacheKey& key, std::ostream* stream) {
        *stream << std::string(key.begin(), key.end());
    }

    namespace {

        std::string RemoveWhitespace(std::string s) {
            s.erase(std::remove_if(
                        s.begin(), s.end(),
                        [](char& c) { return std::isspace<char>(c, std::locale::classic()); }),
                    s.end());
            return s;
        }

        // Matcher to compare CacheKey to a string for easier testing. Note that the string we allow
        // the expectation string to have spaces and newline characters to make it easier for manual
        // validation, but the actual verification strips all whitespace characters.
        MATCHER_P(CacheKeyEq,
                  key,
                  "cache key " + std::string(negation ? "not" : "") + "equal to " +
                      RemoveWhitespace(key)) {
            return std::string(arg.begin(), arg.end()) == RemoveWhitespace(key);
        }

        TEST(CacheKeyPrimitivesTests, IntegralTypes) {
            EXPECT_THAT(GetCacheKey((int)-1), CacheKeyEq("{0:-1}"));
            EXPECT_THAT(GetCacheKey((uint8_t)2), CacheKeyEq("{0:2}"));
            EXPECT_THAT(GetCacheKey((uint16_t)4), CacheKeyEq("{0:4}"));
            EXPECT_THAT(GetCacheKey((uint32_t)8), CacheKeyEq("{0:8}"));
            EXPECT_THAT(GetCacheKey((uint64_t)16), CacheKeyEq("{0:16}"));

            EXPECT_THAT(GetCacheKey((int)-1, (uint8_t)2, (uint16_t)4, (uint32_t)8, (uint64_t)16),
                        CacheKeyEq("{0:-1,1:2,2:4,3:8,4:16}"));
        }

        TEST(CacheKeyPrimitivesTests, FloatingTypes) {
            EXPECT_THAT(GetCacheKey((float)0.5), CacheKeyEq("{0:0.500000}"));
            EXPECT_THAT(GetCacheKey((double)32.0), CacheKeyEq("{0:32.000000}"));

            EXPECT_THAT(GetCacheKey((float)0.5, (double)32.0),
                        CacheKeyEq("{0:0.500000,1:32.000000}"));
        }

        TEST(CacheKeyPrimitivesTests, Strings) {
            std::string str0 = "string0";
            std::string str1 = "string1";

            EXPECT_THAT(GetCacheKey("string0"), CacheKeyEq(R"({0:7"string0"})"));
            EXPECT_THAT(GetCacheKey(str0), CacheKeyEq(R"({0:7"string0"})"));
            EXPECT_THAT(GetCacheKey("string0", str1), CacheKeyEq(R"({0:7"string0",1:7"string1"})"));
        }

        TEST(CacheKeyPrimitivesTests, NestedCacheKey) {
            EXPECT_THAT(GetCacheKey(GetCacheKey((int)-1)), CacheKeyEq("{0:{0:-1}}"));
            EXPECT_THAT(GetCacheKey(GetCacheKey("string")), CacheKeyEq(R"({0:{0:6"string"}})"));
            EXPECT_THAT(GetCacheKey(GetCacheKey(A{})), CacheKeyEq("{0:{0:structA}}"));
            EXPECT_THAT(GetCacheKey(GetCacheKey(B())), CacheKeyEq("{0:{0:classB}}"));

            EXPECT_THAT(GetCacheKey(GetCacheKey((int)-1), GetCacheKey("string"), GetCacheKey(A{}),
                                    GetCacheKey(B())),
                        CacheKeyEq(R"({0:{0:-1},1:{0:6"string"},2:{0:structA},3:{0:classB}})"));
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
