// Copyright 2022 The Tint Authors.
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

#include "src/tint/utils/hashset.h"

#include <array>
#include <random>
#include <string>
#include <tuple>
#include <unordered_set>

#include "gtest/gtest.h"

namespace tint::utils {
namespace {

constexpr std::array kPrimes{
    2,   3,   5,   7,   11,  13,  17,  19,  23,  29,  31,  37,  41,  43,  47,  53,
    59,  61,  67,  71,  73,  79,  83,  89,  97,  101, 103, 107, 109, 113, 127, 131,
    137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
    227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311,
    313, 317, 331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409,
};

TEST(Hashset, Empty) {
    Hashset<std::string> set;
    EXPECT_EQ(set.Count(), 0u);
}

TEST(Hashset, AddRemove) {
    Hashset<std::string> set;
    EXPECT_TRUE(set.Add("hello"));
    EXPECT_EQ(set.Count(), 1u);
    EXPECT_TRUE(set.Contains("hello"));
    EXPECT_FALSE(set.Contains("world"));
    EXPECT_FALSE(set.Add("hello"));
    EXPECT_EQ(set.Count(), 1u);
    EXPECT_TRUE(set.Remove("hello"));
    EXPECT_EQ(set.Count(), 0u);
    EXPECT_FALSE(set.Contains("hello"));
    EXPECT_FALSE(set.Contains("world"));
}

TEST(Hashset, AddMany) {
    Hashset<int> set;
    for (int prime : kPrimes) {
        EXPECT_TRUE(set.Add(prime));
        EXPECT_FALSE(set.Add(prime));
    }
    EXPECT_EQ(set.Count(), kPrimes.size());
    for (int prime : kPrimes) {
        EXPECT_TRUE(set.Contains(prime)) << prime;
    }
}

TEST(Hashset, Soak) {
    std::mt19937 rnd;
    std::unordered_set<std::string> reference;
    Hashset<std::string> set;
    for (size_t i = 0; i < 1000000; i++) {
        std::string value = std::to_string(rnd() & 0x100);
        switch (rnd() % 3) {
            case 0: {
                auto expected = reference.emplace(value).second;
                EXPECT_EQ(set.Add(value), expected);
                break;
            }
            case 1: {
                auto expected = reference.erase(value) != 0;
                EXPECT_EQ(set.Remove(value), expected);
                break;
            }
            case 2: {
                auto expected = reference.count(value) != 0;
                EXPECT_EQ(set.Contains(value), expected);
                break;
            }
        }
    }
}

}  // namespace
}  // namespace tint::utils
