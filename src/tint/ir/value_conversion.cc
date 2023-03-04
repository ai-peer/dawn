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

#include "src/tint/ir/value_conversion.h"
#include "src/tint/debug.h"

TINT_INSTANTIATE_TYPEINFO(tint::ir::ValueConversion);

namespace tint::ir {

ValueConversion::ValueConversion(Value* result,
                                 const type::Type* from,
                                 utils::VectorRef<Value*> args)
    : Base(result), from_(from), args_(args) {
    for (auto* arg : args) {
        arg->AddUsage(this);
    }
}

ValueConversion::~ValueConversion() = default;

utils::StringStream& ValueConversion::ToString(utils::StringStream& out,
                                               const SymbolTable& st) const {
    Result()->ToString(out, st);
    out << " = value_conversion(" << Result()->Type()->FriendlyName(st) << ", "
        << from_->FriendlyName(st);

    for (auto* arg : args_) {
        out << ", ";
        arg->ToString(out, st);
    }
    out << ")";
    return out;
}

}  // namespace tint::ir
