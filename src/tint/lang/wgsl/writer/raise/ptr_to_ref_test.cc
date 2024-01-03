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

#include "src/tint/lang/wgsl/writer/raise/ptr_to_ref.h"

#include <string>
#include <utility>

#include "gtest/gtest.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/disassembler.h"
#include "src/tint/lang/core/ir/validator.h"

namespace tint::wgsl::writer::raise {
namespace {

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

class PtrToRefTest : public testing::Test {
  public:
    /// Transforms the module, using the transforms `TRANSFORMS`.
    void Run() {
        // Validate the input IR.
        {
            auto res = core::ir::Validate(mod);
            EXPECT_EQ(res, Success);
            if (res != Success) {
                return;
            }
        }

        // Run the transforms.
        auto result = PtrToRef(mod);
        EXPECT_EQ(result, Success);

        // Validate the output IR.
        auto res = core::ir::Validate(mod);
        EXPECT_EQ(res, Success);
    }

    /// @returns the transformed module as a disassembled string
    std::string str() {
        core::ir::Disassembler dis(mod);
        return "\n" + dis.Disassemble();
    }

  protected:
    /// The test IR module.
    core::ir::Module mod;
    /// The test IR builder.
    core::ir::Builder b{mod};
    /// The type manager.
    core::type::Manager& ty{mod.Types()};
};

TEST_F(PtrToRefTest, PtrParam_NoChange) {
    auto fn = b.Function(ty.void_());
    fn->SetParams({b.FunctionParam(ty.ptr<function, i32, read_write>())});
    b.Append(fn->Block(), [&] { b.Return(fn); });

    auto* src = R"(
%1 = func(%2:ptr<function, i32, read_write>):void -> %b1 {
  %b1 = block {
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run();

    EXPECT_EQ(expect, str());
}

TEST_F(PtrToRefTest, LoadPtrParam_NoChange) {
    auto fn = b.Function(ty.i32());
    auto* ptr = b.FunctionParam(ty.ptr<function, i32, read_write>());
    fn->SetParams({ptr});
    b.Append(fn->Block(), [&] { b.Return(fn, b.Load(ptr)); });

    auto* src = R"(
%1 = func(%2:ptr<function, i32, read_write>):i32 -> %b1 {
  %b1 = block {
    %3:i32 = load %2
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = src;

    Run();

    EXPECT_EQ(expect, str());
}

TEST_F(PtrToRefTest, Var) {
    b.Append(mod.root_block, [&] { b.Var(ty.ptr<private_, i32>()); });

    auto* src = R"(
%b1 = block {  # root
  %1:ptr<private, i32, read_write> = var
}

)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%b1 = block {  # root
  %1:ref<private, i32, read_write> = var
}

)";

    Run();

    EXPECT_EQ(expect, str());
}

TEST_F(PtrToRefTest, LoadVar) {
    auto fn = b.Function(ty.i32());
    b.Append(fn->Block(), [&] {
        auto* v = b.Var<function, i32>();
        b.Return(fn, b.Load(v));
    });

    auto* src = R"(
%1 = func():i32 -> %b1 {
  %b1 = block {
    %2:ptr<function, i32, read_write> = var
    %3:i32 = load %2
    ret %3
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%1 = func():i32 -> %b1 {
  %b1 = block {
    %2:ref<function, i32, read_write> = var
    %3:i32 = load %2
    ret %3
  }
}
)";

    Run();

    EXPECT_EQ(expect, str());
}

TEST_F(PtrToRefTest, StoreVar) {
    auto fn = b.Function(ty.void_());
    b.Append(fn->Block(), [&] {
        auto* v = b.Var<function, i32>();
        b.Store(v, 42_i);
        b.Return(fn);
    });

    auto* src = R"(
%1 = func():void -> %b1 {
  %b1 = block {
    %2:ptr<function, i32, read_write> = var
    store %2, 42i
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%1 = func():void -> %b1 {
  %b1 = block {
    %2:ref<function, i32, read_write> = var
    store %2, 42i
    ret
  }
}
)";

    Run();

    EXPECT_EQ(expect, str());
}

TEST_F(PtrToRefTest, VarUsedAsPtrParam) {
    auto fn_a = b.Function(ty.void_());
    fn_a->SetParams({b.FunctionParam<ptr<function, i32, read_write>>("p")});
    b.Append(fn_a->Block(), [&] { b.Return(fn_a); });
    auto fn_b = b.Function(ty.void_());
    b.Append(fn_b->Block(), [&] {
        auto* v = b.Var<function, i32>();
        b.Call(fn_a, v);
        b.Return(fn_b);
    });

    auto* src = R"(
%1 = func(%p:ptr<function, i32, read_write>):void -> %b1 {
  %b1 = block {
    ret
  }
}
%3 = func():void -> %b2 {
  %b2 = block {
    %4:ptr<function, i32, read_write> = var
    %5:void = call %1, %4
    ret
  }
}
)";
    EXPECT_EQ(src, str());

    auto* expect = R"(
%1 = func(%p:ptr<function, i32, read_write>):void -> %b1 {
  %b1 = block {
    ret
  }
}
%3 = func():void -> %b2 {
  %b2 = block {
    %4:ref<function, i32, read_write> = var
    %5:ptr<function, i32, read_write> = ref-to-ptr %4
    %6:void = call %1, %5
    ret
  }
}
)";

    Run();

    EXPECT_EQ(expect, str());
}

}  // namespace
}  // namespace tint::wgsl::writer::raise
