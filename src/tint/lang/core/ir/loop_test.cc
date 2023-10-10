// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/core/ir/loop.h"
#include "gtest/gtest-spi.h"
#include "src/tint/lang/core/ir/ir_helper_test.h"

namespace tint::core::ir {
namespace {

using namespace tint::core::number_suffixes;  // NOLINT
using IR_LoopTest = IRTestHelper;

TEST_F(IR_LoopTest, Parent) {
    auto* loop = b.Loop();
    EXPECT_EQ(loop->Initializer()->Parent(), loop);
    EXPECT_EQ(loop->Body()->Parent(), loop);
    EXPECT_EQ(loop->Continuing()->Parent(), loop);
}

TEST_F(IR_LoopTest, Result) {
    auto* loop = b.Loop();
    EXPECT_FALSE(loop->HasResults());
    EXPECT_FALSE(loop->HasMultiResults());
}

TEST_F(IR_LoopTest, Fail_NullInitializerBlock) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            Loop loop(nullptr, b.MultiInBlock(), b.MultiInBlock());
        },
        "");
}

TEST_F(IR_LoopTest, Fail_NullBodyBlock) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            Loop loop(b.Block(), nullptr, b.MultiInBlock());
        },
        "");
}

TEST_F(IR_LoopTest, Fail_NullContinuingBlock) {
    EXPECT_FATAL_FAILURE(
        {
            Module mod;
            Builder b{mod};
            Loop loop(b.Block(), b.MultiInBlock(), nullptr);
        },
        "");
}

TEST_F(IR_LoopTest, Clone) {
    auto* loop = b.Loop();
    auto* new_loop = clone_ctx.Clone(loop);

    EXPECT_NE(loop, new_loop);
    EXPECT_FALSE(new_loop->HasResults());
    EXPECT_EQ(0u, new_loop->Exits().Count());
    EXPECT_NE(nullptr, new_loop->Initializer());
    EXPECT_NE(loop->Initializer(), new_loop->Initializer());

    EXPECT_NE(nullptr, new_loop->Body());
    EXPECT_NE(loop->Body(), new_loop->Body());

    EXPECT_NE(nullptr, new_loop->Continuing());
    EXPECT_NE(loop->Continuing(), new_loop->Continuing());
}

TEST_F(IR_LoopTest, CloneWithExits) {
    Loop* new_loop = nullptr;
    {
        auto* loop = b.Loop();
        b.Append(loop->Body(), [&] {
            auto* if_ = b.If(true);
            b.Append(if_->True(), [&] { b.Continue(loop); });
            b.Append(if_->False(), [&] { b.ExitLoop(loop); });
            b.Append(loop->Continuing(), [&] { b.BreakIf(loop, false); });

            b.NextIteration(loop);
        });
        new_loop = clone_ctx.Clone(loop);
    }

    ASSERT_EQ(2u, new_loop->Body()->Length());
    EXPECT_TRUE(new_loop->Body()->Front()->Is<If>());

    auto* new_if = new_loop->Body()->Front()->As<If>();
    ASSERT_EQ(1u, new_if->True()->Length());
    EXPECT_TRUE(new_if->True()->Front()->Is<Continue>());
    EXPECT_EQ(new_loop, new_if->True()->Front()->As<Continue>()->Loop());

    ASSERT_EQ(1u, new_if->False()->Length());
    EXPECT_TRUE(new_if->False()->Front()->Is<ExitLoop>());
    EXPECT_EQ(new_loop, new_if->False()->Front()->As<ExitLoop>()->Loop());

    ASSERT_EQ(1u, new_loop->Continuing()->Length());
    EXPECT_TRUE(new_loop->Continuing()->Front()->Is<BreakIf>());
    EXPECT_EQ(new_loop, new_loop->Continuing()->Front()->As<BreakIf>()->Loop());

    EXPECT_TRUE(new_loop->Body()->Back()->Is<NextIteration>());
    EXPECT_EQ(new_loop, new_loop->Body()->Back()->As<NextIteration>()->Loop());
}

}  // namespace
}  // namespace tint::core::ir
