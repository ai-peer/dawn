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

#include "src/tint/cmd/fuzz/ir/ir_fuzz.h"

#include <iostream>
#include <thread>

#include "src/tint/utils/containers/vector.h"
#include "src/tint/utils/macros/defer.h"
#include "src/tint/utils/macros/scoped_assignment.h"
#include "src/tint/utils/macros/static_init.h"

namespace tint::fuzz::ir {
namespace {

Vector<IRFuzzer, 32> fuzzers;
thread_local std::string_view currently_running;

[[noreturn]] void TintInternalCompilerErrorReporter(const tint::InternalCompilerError& err) {
    std::cerr << "ICE while running fuzzer: '" << currently_running << "'" << std::endl;
    std::cerr << err.Error() << std::endl;
    __builtin_trap();
}

}  // namespace

void Register(const IRFuzzer& fuzzer) {
    fuzzers.Push(fuzzer);
}

bool Run(uint8_t id, ::tint::core::ir::Module& mod) {
    if (id >= fuzzers.Length()) {
        return false;
    }

    tint::SetInternalCompilerErrorReporter(&TintInternalCompilerErrorReporter);

    // Ensure that fuzzers are sorted. Without this, the fuzzers may be registered in any order,
    // leading to non-determinism, which we must avoid.
    TINT_STATIC_INIT(fuzzers.Sort([](auto& a, auto& b) { return a.name < b.name; }));

    auto& fuzzer = fuzzers[id];
    {
        TINT_SCOPED_ASSIGNMENT(currently_running, fuzzer.name);
        fuzzer.fn(mod);
    }

    return true;
}

}  // namespace tint::fuzz::ir
