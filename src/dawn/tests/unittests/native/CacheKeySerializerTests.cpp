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

namespace dawn::native { namespace {

    using ::testing::StrEq;

    TEST(CacheKeySerializerTest, FormatGenerator) {
        EXPECT_THAT(detail::CacheKeyFormatGenerator<1>::value, StrEq("{%u:%#s}"));
        EXPECT_THAT(detail::CacheKeyFormatGenerator<2>::value, StrEq("{%u:%#s,%u:%#s}"));
        EXPECT_THAT(detail::CacheKeyFormatGenerator<3>::value, StrEq("{%u:%#s,%u:%#s,%u:%#s}"));
    }

    TEST(CacheKeySerializerTest, FormatCharWrapper) {
        EXPECT_EQ(detail::CacheKeyFormatCharWrapper<0>::value, 'u');
        EXPECT_EQ(detail::CacheKeyFormatCharWrapper<1>::value, 's');
        EXPECT_EQ(detail::CacheKeyFormatCharWrapper<2>::value, 'u');
        EXPECT_EQ(detail::CacheKeyFormatCharWrapper<3>::value, 's');
    }

    TEST(CacheKeySerializerTest, Enumerate) {
        EXPECT_EQ(detail::Enumerate("hello"), std::make_tuple(0, "hello"));
        EXPECT_EQ(detail::Enumerate(1, 1.0, "hello"), std::make_tuple(0, 1, 1, 1.0, 2, "hello"));
    }

    struct A {};
    absl::FormatConvertResult<absl::FormatConversionCharSet::kString>
    AbslFormatConvert(const A& value, const absl::FormatConversionSpec& spec, absl::FormatSink* s) {
        if (spec.has_alt_flag()) {
            s->Append("Struct A");
        } else {
            s->Append("Unexpected");
        }
        return {true};
    }

    class B {};
    absl::FormatConvertResult<absl::FormatConversionCharSet::kString>
    AbslFormatConvert(const B& value, const absl::FormatConversionSpec& spec, absl::FormatSink* s) {
        if (spec.has_alt_flag()) {
            s->Append("Class B");
        } else {
            s->Append("Unexpected");
        }
        return {true};
    }

    TEST(CacheKeySerializerTest, Serialize) {
        EXPECT_THAT(CacheKeySerializer(std::to_string(20)), StrEq("{0:20}"));
        EXPECT_THAT(CacheKeySerializer(std::to_string(1.0)), StrEq("{0:1.000000}"));
        EXPECT_THAT(CacheKeySerializer("string"), StrEq("{0:string}"));
        EXPECT_THAT(CacheKeySerializer(std::to_string(20), std::to_string(1.0), "string", A{}, B()),
                    StrEq("{0:20,1:1.000000,2:string,3:Struct A,4:Class B}"));
    }

}}  // namespace dawn::native::
