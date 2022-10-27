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

#include "src/tint/ir/builder.h"

#include "src/tint/ir/builder_impl.h"
#include "src/tint/program.h"

namespace tint::ir {

using ResultType = utils::Result<Module, std::string>;

// static
ResultType Builder::Build(const Program* program) {
    if (!program->IsValid()) {
        return ResultType{std::string("input program is not valid")};
    }

    BuilderImpl b(program);
    auto r = b.Build();
    if (!r) {
        return b.error();
    }

    return ResultType{r.Move()};
}

}  // namespace tint::ir
