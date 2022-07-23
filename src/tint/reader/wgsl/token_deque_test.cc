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

#include "src/tint/reader/wgsl/token_deque.h"

#include <vector>

#include "gtest/gtest.h"

#include "src/tint/source.h"

namespace tint::reader::wgsl {
namespace {

using TokenDequeTest = testing::Test;

TEST_F(TokenDequeTest, create) {
    TokenDeque d;
    EXPECT_TRUE(d.empty());
    EXPECT_EQ(0u, d.size());
}

struct TokenData {
    Token::Type type;
    Source source;
};

TEST_F(TokenDequeTest, push_and_pop_front) {
    std::vector<TokenData> tokens = {
        {Token::Type::kReturn, Source({20, 2})},
        {Token::Type::kContinue, Source({10, 4})},
        {Token::Type::kBreak, Source({30, 6})},
    };

    TokenDeque d;
    for (auto& t : tokens) {
        d.push_front(Token(t.type, t.source));
    }

    EXPECT_FALSE(d.empty());
    EXPECT_EQ(std::size(tokens), d.size());

    auto s = d.size();
    for (auto iter = tokens.rbegin(); iter != tokens.rend(); iter++) {
        Token t = d.pop_front();
        TokenData r = *iter;

        EXPECT_TRUE(t.Is(r.type));
        EXPECT_EQ(r.source.range.begin, t.source().range.begin);

        s--;
        if (iter != (tokens.rend() - 1)) {
            EXPECT_FALSE(d.empty());
            EXPECT_EQ(s, d.size());
        }
    }

    EXPECT_TRUE(d.empty());
    EXPECT_EQ(0u, d.size());
}

TEST_F(TokenDequeTest, push_back) {
    std::vector<TokenData> tokens = {
        {Token::Type::kReturn, Source({20, 2})},
        {Token::Type::kContinue, Source({10, 4})},
        {Token::Type::kBreak, Source({30, 6})},
    };

    TokenDeque d;
    for (auto& t : tokens) {
        d.push_back(Token(t.type, t.source));
    }

    EXPECT_FALSE(d.empty());
    EXPECT_EQ(std::size(tokens), d.size());

    for (size_t i = 0; i < std::size(tokens); i++) {
        auto t = d.pop_front();
        EXPECT_TRUE(t.Is(tokens[i].type));
        EXPECT_EQ(tokens[i].source.range.begin, t.source().range.begin);

        if (i != tokens.size() - 1) {
            EXPECT_FALSE(d.empty());
            EXPECT_EQ(tokens.size() - i - 1, d.size());
        }
    }

    EXPECT_TRUE(d.empty());
    EXPECT_EQ(0u, d.size());
}

TEST_F(TokenDequeTest, push_back_lots) {
    TokenDeque d;
    for (size_t i = 0; i < 100; i++) {
        d.push_back(Token(Token::Type::kReturn, Source({20, 2})));
        EXPECT_EQ(1u, d.size());

        EXPECT_TRUE(d[0].Is(Token::Type::kReturn));

        d.pop_front();
        EXPECT_EQ(0u, d.size());
    }
}

TEST_F(TokenDequeTest, operator_accessor) {
    std::vector<TokenData> tokens = {
        {Token::Type::kReturn, Source({20, 2})},
        {Token::Type::kContinue, Source({10, 4})},
        {Token::Type::kBreak, Source({30, 6})},
    };

    TokenDeque d;
    for (auto& t : tokens) {
        d.push_back(Token(t.type, t.source));
    }

    EXPECT_FALSE(d.empty());
    EXPECT_EQ(std::size(tokens), d.size());

    for (size_t i = 0; i < std::size(tokens); i++) {
        EXPECT_TRUE(d[i].Is(tokens[i].type));
        EXPECT_EQ(tokens[i].source.range.begin, d[i].source().range.begin);
    }

    EXPECT_FALSE(d.empty());
    EXPECT_EQ(std::size(tokens), d.size());
}

}  // namespace
}  // namespace tint::reader::wgsl
