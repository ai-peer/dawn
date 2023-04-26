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

#include "src/tint/transform/integer_value_range_analysis.h"

#include <utility>

#include "src/tint/ast/workgroup_attribute.h"
#include "src/tint/builtin/builtin_value.h"
#include "src/tint/program_builder.h"
#include "src/tint/sem/function.h"
#include "src/tint/sem/variable.h"

namespace tint::transform {

IntegerValueRangeAnalysis::IntegerValueRangeAnalysis() = default;
IntegerValueRangeAnalysis::~IntegerValueRangeAnalysis() = default;

/// PIMPL state for the analysis
struct IntegerValueRangeAnalysis::State {
    /// Constructor
    /// @param program the source program
    explicit State(const Program* program) : src(program) {}

    /// Collect the range of builtin(local_invocation_id)
    /// @param astFunction the compute stage function
    void CollectLocalInvocationID(const tint::ast::Function* astFunction) {
        auto& sem = src->Sem();

        const ast::WorkgroupAttribute* attr =
            ast::GetAttribute<ast::WorkgroupAttribute>(astFunction->attributes);
        TINT_ASSERT(Transform, attr != nullptr);

        const auto& workgroupSizeValues = attr->Values();
        std::vector<IntegerValueRange> localInvocationIndexRange(workgroupSizeValues.size());
        for (size_t i = 0; i < workgroupSizeValues.size(); ++i) {
            localInvocationIndexRange[i].minValue = 0;

            const auto* expr = workgroupSizeValues[i];
            if (!expr) {
                localInvocationIndexRange[i].maxValue = 0;
                continue;
            }

            auto* workgroupSizeSem = src->Sem().GetVal(expr);
            if (auto* c = workgroupSizeSem->ConstantValue()) {
                localInvocationIndexRange[i].maxValue = c->ValueAs<AInt>() - 1;
            }
        }

        for (auto* param : astFunction->params) {
            if (auto* builtin_attr = ast::GetAttribute<ast::BuiltinAttribute>(param->attributes)) {
                auto builtin = sem.Get(builtin_attr)->Value();
                if (builtin == builtin::BuiltinValue::kLocalInvocationId) {
                    ranged_integer_variables.emplace(
                        std::make_pair(sem.Get(param), localInvocationIndexRange));
                }
            }
        }
    }

    /// Runs the analysis
    /// @returns the result that stores all the integer variables and their ranges
    RangedIntegerVariablesMap Run() {
        for (auto* fn : src->AST().Functions()) {
            if (fn->PipelineStage() == ast::PipelineStage::kCompute) {
                CollectLocalInvocationID(fn);
            }
        }

        return ranged_integer_variables;
    }

  private:
    /// The source program
    const Program* const src;
    /// The map that stores all the integer variables and their ranges
    RangedIntegerVariablesMap ranged_integer_variables;
};

IntegerValueRangeAnalysis::RangedIntegerVariablesMap IntegerValueRangeAnalysis::Apply(
    const Program* src) const {
    return State{src}.Run();
}

}  // namespace tint::transform
