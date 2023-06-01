// Copyright 2023 The Tint Authors.
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

#include "src/tint/ir/block.h"
#include "gtest/gtest-spi.h"
#include "gtest/gtest.h"
#include "src/tint/ir/builder.h"
#include "src/tint/ir/module.h"

namespace tint::ir {
namespace {

class IR_BlockTest : public ::testing::Test {
  public:
    Module mod;
    Builder b{mod};
};

TEST_F(IR_BlockTest, SetInstructions) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst3 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst1, inst2, inst3});

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    ASSERT_EQ(inst3->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(3u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    inst = inst->next;

    ASSERT_EQ(inst, inst3);
    ASSERT_EQ(inst->prev, inst2);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Append) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst3 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->Append(inst1);
    blk->Append(inst2);
    blk->Append(inst3);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    ASSERT_EQ(inst3->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(3u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    inst = inst->next;

    ASSERT_EQ(inst, inst3);
    ASSERT_EQ(inst->prev, inst2);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Prepend) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst3 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->Prepend(inst3);
    blk->Prepend(inst2);
    blk->Prepend(inst1);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    ASSERT_EQ(inst3->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(3u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    inst = inst->next;

    ASSERT_EQ(inst, inst3);
    ASSERT_EQ(inst->prev, inst2);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, InsertBefore_AtStart) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->Append(inst2);
    blk->InsertBefore(inst2, inst1);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(2u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, InsertBefore_Middle) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst3 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->Append(inst1);
    blk->Append(inst3);
    blk->InsertBefore(inst3, inst2);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    ASSERT_EQ(inst3->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(3u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    inst = inst->next;

    ASSERT_EQ(inst, inst3);
    ASSERT_EQ(inst->prev, inst2);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, InsertAfter_AtEnd) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->Append(inst1);
    blk->InsertAfter(inst1, inst2);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(2u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, InsertAfter_Middle) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst3 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->Append(inst1);
    blk->Append(inst3);
    blk->InsertAfter(inst1, inst2);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    ASSERT_EQ(inst3->Block(), blk);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(3u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    inst = inst->next;

    ASSERT_EQ(inst, inst3);
    ASSERT_EQ(inst->prev, inst2);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Replace_Middle) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst3 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst1, inst4, inst3});
    blk->Replace(inst4, inst2);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    ASSERT_EQ(inst3->Block(), blk);
    EXPECT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(3u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    inst = inst->next;

    ASSERT_EQ(inst, inst3);
    ASSERT_EQ(inst->prev, inst2);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Replace_Start) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst4, inst2});
    blk->Replace(inst4, inst1);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    EXPECT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(2u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Replace_End) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst1, inst4});
    blk->Replace(inst4, inst2);

    ASSERT_EQ(inst1->Block(), blk);
    ASSERT_EQ(inst2->Block(), blk);
    EXPECT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(2u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Replace_OnlyNode) {
    auto* inst1 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst4});
    blk->Replace(inst4, inst1);

    ASSERT_EQ(inst1->Block(), blk);
    EXPECT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(1u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Remove_Middle) {
    auto* inst1 = b.CreateLoop();
    auto* inst2 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst1, inst4, inst2});
    blk->Remove(inst4);

    ASSERT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(2u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    inst = inst->next;

    ASSERT_EQ(inst, inst2);
    ASSERT_EQ(inst->prev, inst1);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Remove_Start) {
    auto* inst1 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst4, inst1});
    blk->Remove(inst4);

    ASSERT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(1u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Remove_End) {
    auto* inst1 = b.CreateLoop();
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst1, inst4});
    blk->Remove(inst4);

    ASSERT_EQ(inst4->Block(), nullptr);

    EXPECT_FALSE(blk->IsEmpty());
    EXPECT_EQ(1u, blk->Length());

    auto* inst = blk->Instructions();
    ASSERT_EQ(inst, inst1);
    ASSERT_EQ(inst->prev, nullptr);
    ASSERT_EQ(inst->next, nullptr);
}

TEST_F(IR_BlockTest, Remove_OnlyNode) {
    auto* inst4 = b.CreateLoop();

    auto* blk = b.CreateBlock();
    blk->SetInstructions(utils::Vector{inst4});
    blk->Remove(inst4);

    ASSERT_EQ(inst4->Block(), nullptr);

    EXPECT_TRUE(blk->IsEmpty());
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_PrependNullptr) {
    static auto* blk = b.CreateBlock();
    EXPECT_FATAL_FAILURE(blk->Prepend(nullptr), "");
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_PrependAlreadyInserted) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    blk->Prepend(inst1);
    EXPECT_FATAL_FAILURE(blk->Prepend(inst1), "");
    EXPECT_EQ(1u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_AppendNullptr) {
    static auto* blk = b.CreateBlock();
    EXPECT_FATAL_FAILURE(blk->Append(nullptr), "");
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_AppendAlreadyInserted) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    blk->Append(inst1);
    EXPECT_FATAL_FAILURE(blk->Append(inst1), "");
    EXPECT_EQ(1u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_InsertBeforeNullptrInst) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    EXPECT_FATAL_FAILURE(blk->InsertBefore(nullptr, inst1), "");
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_InsertBeforeInstNullptr) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    blk->Append(inst1);
    EXPECT_FATAL_FAILURE(blk->InsertBefore(inst1, nullptr), "");
    EXPECT_EQ(1u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_InsertBeforeDifferentBlock) {
    static auto* inst1 = b.CreateLoop();
    static auto* inst2 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    auto* blk2 = b.CreateBlock();
    blk2->Append(inst1);
    EXPECT_FATAL_FAILURE(blk1->InsertBefore(inst1, inst2), "");
    EXPECT_EQ(0u, blk1->Length());
    EXPECT_EQ(1u, blk2->Length());
    EXPECT_EQ(inst2->Block(), nullptr);
}

TEST_F(IR_BlockTest, Fail_InsertBeforeAlreadyInserted) {
    static auto* inst1 = b.CreateLoop();
    static auto* inst2 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    blk1->Append(inst1);
    blk1->Append(inst2);
    EXPECT_FATAL_FAILURE(blk1->InsertBefore(inst1, inst2), "");
    EXPECT_EQ(2u, blk1->Length());
    EXPECT_EQ(inst2->Block(), blk1);
}

TEST_F(IR_BlockTest, Fail_InsertAfterNullptrInst) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    EXPECT_FATAL_FAILURE(blk->InsertAfter(nullptr, inst1), "");
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_InsertAfterInstNullptr) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    blk->Append(inst1);
    EXPECT_FATAL_FAILURE(blk->InsertAfter(inst1, nullptr), "");
    EXPECT_EQ(1u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_InsertAfterDifferentBlock) {
    static auto* inst1 = b.CreateLoop();
    static auto* inst2 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    auto* blk2 = b.CreateBlock();
    blk2->Append(inst1);
    EXPECT_FATAL_FAILURE(blk1->InsertAfter(inst1, inst2), "");
    EXPECT_EQ(0u, blk1->Length());
    EXPECT_EQ(1u, blk2->Length());
    EXPECT_EQ(inst2->Block(), nullptr);
}

TEST_F(IR_BlockTest, Fail_InsertAfterAlreadyInserted) {
    static auto* inst1 = b.CreateLoop();
    static auto* inst2 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    blk1->Append(inst1);
    blk1->Append(inst2);
    EXPECT_FATAL_FAILURE(blk1->InsertAfter(inst1, inst2), "");
    EXPECT_EQ(2u, blk1->Length());
    EXPECT_EQ(inst2->Block(), blk1);
}

TEST_F(IR_BlockTest, Fail_ReplaceNullptrInst) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    EXPECT_FATAL_FAILURE(blk->Replace(nullptr, inst1), "");
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_ReplaceInstNullptr) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk = b.CreateBlock();
    blk->Append(inst1);
    EXPECT_FATAL_FAILURE(blk->Replace(inst1, nullptr), "");
    EXPECT_EQ(1u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_ReplaceDifferentBlock) {
    static auto* inst1 = b.CreateLoop();
    static auto* inst2 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    auto* blk2 = b.CreateBlock();
    blk2->Append(inst1);
    EXPECT_FATAL_FAILURE(blk1->Replace(inst1, inst2), "");
    EXPECT_EQ(0u, blk1->Length());
    EXPECT_EQ(1u, blk2->Length());
    EXPECT_EQ(inst2->Block(), nullptr);
}

TEST_F(IR_BlockTest, Fail_ReplaceAlreadyInserted) {
    static auto* inst1 = b.CreateLoop();
    static auto* inst2 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    blk1->Append(inst1);
    blk1->Append(inst2);
    EXPECT_FATAL_FAILURE(blk1->Replace(inst1, inst2), "");
    EXPECT_EQ(2u, blk1->Length());
    EXPECT_EQ(inst2->Block(), blk1);
}

TEST_F(IR_BlockTest, Fail_RemoveNullptr) {
    static auto* blk = b.CreateBlock();
    EXPECT_FATAL_FAILURE(blk->Remove(nullptr), "");
    EXPECT_EQ(0u, blk->Length());
}

TEST_F(IR_BlockTest, Fail_RemoveDifferentBlock) {
    static auto* inst1 = b.CreateLoop();
    static auto* blk1 = b.CreateBlock();
    auto* blk2 = b.CreateBlock();
    blk2->Append(inst1);
    EXPECT_FATAL_FAILURE(blk1->Remove(inst1), "");
    EXPECT_EQ(0u, blk1->Length());
    EXPECT_EQ(1u, blk2->Length());
}

}  // namespace
}  // namespace tint::ir
