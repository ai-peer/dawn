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

#include "dawn/native/CacheKey.h"

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

    }  // namespace

}  // namespace dawn::native
