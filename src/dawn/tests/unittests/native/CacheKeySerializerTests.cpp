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

#include <string>

#include "dawn/native/CacheKeySerializer.h"

namespace dawn::native {

    // Testing classes/structs with serializing implemented for testing.
    struct A {};
    void CacheKeySerialize(CacheKey* key, const A& t) {
        std::string str = "structA";
        key->insert(key->end(), str.begin(), str.end());
    }

    class B {};
    void CacheKeySerialize(CacheKey* key, const B& t) {
        std::string str = "classB";
        key->insert(key->end(), str.begin(), str.end());
    }

    namespace {

        // Matcher to compare CacheKey to a string for easier testing.
        MATCHER_P(CacheKeyEq,
                  key,
                  "cache key " + std::string(negation ? "not" : "") + "equal to " + key) {
            return std::string(arg.begin(), arg.end()) == key;
        }

        TEST(CacheKeySerializerTest, IntegralTypes) {
            EXPECT_THAT(CacheKeySerializer((int)-1), CacheKeyEq("{0:-1}"));
            EXPECT_THAT(CacheKeySerializer((uint8_t)2), CacheKeyEq("{0:2}"));
            EXPECT_THAT(CacheKeySerializer((uint16_t)4), CacheKeyEq("{0:4}"));
            EXPECT_THAT(CacheKeySerializer((uint32_t)8), CacheKeyEq("{0:8}"));
            EXPECT_THAT(CacheKeySerializer((uint64_t)16), CacheKeyEq("{0:16}"));

            EXPECT_THAT(
                CacheKeySerializer((int)-1, (uint8_t)2, (uint16_t)4, (uint32_t)8, (uint64_t)16),
                CacheKeyEq("{0:-1,1:2,2:4,3:8,4:16}"));
        }

        TEST(CacheKeySerializerTest, Strings) {
            EXPECT_THAT(CacheKeySerializer("string"), CacheKeyEq("{0:\"string\"}"));
            EXPECT_THAT(CacheKeySerializer("string0", "string1", "string2"),
                        CacheKeyEq("{0:\"string0\",1:\"string1\",2:\"string2\"}"));
        }

        TEST(CacheKeySerializerTest, NestedCacheKey) {
            EXPECT_THAT(CacheKeySerializer(CacheKeySerializer((int)-1)), CacheKeyEq("{0:{0:-1}}"));
            EXPECT_THAT(CacheKeySerializer(CacheKeySerializer("string")),
                        CacheKeyEq("{0:{0:\"string\"}}"));
            EXPECT_THAT(CacheKeySerializer(CacheKeySerializer(A{})), CacheKeyEq("{0:{0:structA}}"));
            EXPECT_THAT(CacheKeySerializer(CacheKeySerializer(B())), CacheKeyEq("{0:{0:classB}}"));

            EXPECT_THAT(
                CacheKeySerializer(CacheKeySerializer((int)-1), CacheKeySerializer("string"),
                                   CacheKeySerializer(A{}), CacheKeySerializer(B())),
                CacheKeyEq("{0:{0:-1},1:{0:\"string\"},2:{0:structA},3:{0:classB}}"));
        }

    }  // namespace
}  // namespace dawn::native
