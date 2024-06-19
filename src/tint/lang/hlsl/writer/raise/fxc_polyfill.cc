// Copyright 2024 The Dawn & Tint Authors
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

#include "src/tint/lang/hlsl/writer/raise/fxc_polyfill.h"

#include "src/tint/lang/core/ir/block.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/exit.h"
#include "src/tint/lang/core/ir/loop.h"
#include "src/tint/lang/core/ir/switch.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/lang/core/type/manager.h"
#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/ice/ice.h"
#include "src/tint/utils/result/result.h"

namespace tint::hlsl::writer::raise {
namespace {

using namespace tint::core::fluent_types;  // NOLINT

/// PIMPL state for the transform.
struct State {
    /// The IR module.
    core::ir::Module& ir;

    /// The IR builder.
    core::ir::Builder b{ir};

    /// The type manager.
    core::type::Manager& ty{ir.Types()};

    /// Process the module.
    void Process() {
        Vector<core::ir::Switch*, 4> switch_worklist;
        for (auto* inst : ir.Instructions()) {
            if (auto* swtch = inst->As<core::ir::Switch>()) {
                // Switch with only a default selector
                if (swtch->Cases().Length() == 1 && swtch->Cases()[0].selectors.Length() == 1 &&
                    swtch->Cases()[0].selectors[0].IsDefault()) {
                    switch_worklist.Push(swtch);
                }
            }
        }

        // Polyfill the switches that we found
        for (auto* swtch : switch_worklist) {
            ReplaceDefaultOnlySwitchWithLoop(swtch);
        }
    }

    // BUG(crbug.com/tint/1188): work around default-only switches
    //
    // FXC fails to compile a switch with just a default case, ignoring the
    // default case body. We work around this here by emitting the default case
    // without the switch. The case is emitted into a while loop, this means a `break` in the switch
    // will work correctly.
    void ReplaceDefaultOnlySwitchWithLoop(core::ir::Switch* swtch) {
        auto body = swtch->Cases()[0].block;

        core::ir::Loop* l = b.Loop();
        l->InsertBefore(swtch);

        // Retarget ExitSwitch to ExitLoop ...
        Vector<core::ir::Exit*, 4> exits;
        for (auto exit : swtch->Exits()) {
            exits.Push(exit);
        }

        // Convert all the EndSwitch statements to EndLoop statements. Do this before splicing so
        // the blocks don't get changed
        for (auto exit : exits) {
            b.InsertBefore(exit, [&] { b.ExitLoop(l); });
            exit->Destroy();
        }

        TINT_ASSERT(!body->IsEmpty());

        // Splice the body of the default into the new loop
        body->SpliceRangeIntoBlock(body->Front(), body->Back(), l->Body());
        swtch->Destroy();
    }
};

}  // namespace

Result<SuccessType> FxcPolyfill(core::ir::Module& ir) {
    auto result = ValidateAndDumpIfNeeded(ir, "FxcPolyfill transform");
    if (result != Success) {
        return result.Failure();
    }

    State{ir}.Process();

    return Success;
}

}  // namespace tint::hlsl::writer::raise
