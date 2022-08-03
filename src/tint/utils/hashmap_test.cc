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

#include "src/tint/utils/hashmap.h"

#include <array>
#include <random>
#include <string>
#include <tuple>
#include <unordered_map>

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

TEST(Hashmap, Empty) {
    Hashmap<std::string, int> map;
    EXPECT_EQ(map.Count(), 0u);
}

TEST(Hashmap, AddRemove) {
    Hashmap<std::string, std::string> map;
    EXPECT_TRUE(map.Add("hello", "world"));
    EXPECT_EQ(map.Count(), 1u);
    EXPECT_TRUE(map.Contains("hello"));
    EXPECT_FALSE(map.Contains("world"));
    EXPECT_FALSE(map.Add("hello", "cat"));
    EXPECT_EQ(map.Count(), 1u);
    EXPECT_TRUE(map.Remove("hello"));
    EXPECT_EQ(map.Count(), 0u);
    EXPECT_FALSE(map.Contains("hello"));
    EXPECT_FALSE(map.Contains("world"));
}

TEST(Hashmap, AddMany) {
    Hashmap<int, std::string> map;
    for (int prime : kPrimes) {
        EXPECT_TRUE(map.Add(prime, std::to_string(prime)));
        EXPECT_FALSE(map.Add(prime, std::to_string(prime)));
    }
    EXPECT_EQ(map.Count(), kPrimes.size());
    for (int prime : kPrimes) {
        EXPECT_TRUE(map.Contains(prime)) << prime;
        EXPECT_EQ(map.Get(prime), std::to_string(prime)) << prime;
    }
}

TEST(Hashmap, Soak) {
    std::mt19937 rnd;
    std::unordered_map<std::string, std::string> reference;
    Hashmap<std::string, std::string> map;
    for (size_t i = 0; i < 1000000; i++) {
        std::string key = std::to_string(rnd() & 0x100);
        std::string value = "V" + key;
        switch (rnd() % 4) {
            case 0: {
                auto expected = reference.emplace(key, value).second;
                EXPECT_EQ(map.Add(key, value), expected);
                break;
            }
            case 1: {
                auto expected = reference.erase(key) != 0;
                EXPECT_EQ(map.Remove(key), expected);
                break;
            }
            case 2: {
                auto expected = reference.count(key) != 0;
                EXPECT_EQ(map.Contains(key), expected);
                break;
            }
            case 3: {
                if (reference.count(key) != 0) {
                    auto expected = reference[key];
                    EXPECT_EQ(map.Get(key), expected);
                } else {
                    EXPECT_FALSE(map.Get(key).has_value());
                }
                break;
            }
        }
    }
}

}  // namespace
}  // namespace tint::utils
