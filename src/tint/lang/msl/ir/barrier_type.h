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

#ifndef SRC_TINT_LANG_MSL_IR_BARRIER_TYPE_H_
#define SRC_TINT_LANG_MSL_IR_BARRIER_TYPE_H_

#include "src/tint/lang/core/ir/clone_context.h"
#include "src/tint/lang/core/ir/value.h"
#include "src/tint/lang/msl/barrier_type.h"
#include "src/tint/utils/rtti/castable.h"

namespace tint::msl::ir {

/// BarrierType is a value corresponding to an entry from the BarrierType enum
class BarrierType final : public Castable<BarrierType, core::ir::Value> {
  public:
    /// Constructor
    /// @param type the type
    /// @param barrier_type the barrier type
    BarrierType(const core::type::Type* type, msl::BarrierType barrier_type);
    /// Destructor
    ~BarrierType() override;

    /// @returns the type of the constant
    const core::type::Type* Type() const override { return type_; }

    /// @copydoc core::ir::Value::Clone()
    BarrierType* Clone(core::ir::CloneContext& ctx) override;

  private:
    const core::type::Type* type_;
    msl::BarrierType barrier_type_;
};

}  // namespace tint::msl::ir

#endif  // SRC_TINT_LANG_MSL_IR_BARRIER_TYPE_H_
