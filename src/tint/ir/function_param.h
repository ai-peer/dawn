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

#ifndef SRC_TINT_IR_FUNCTION_PARAM_H_
#define SRC_TINT_IR_FUNCTION_PARAM_H_

#include "src/tint/ir/value.h"
#include "src/tint/utils/castable.h"

namespace tint::ir {

/// A function parameter in the IR.
class FunctionParam : public utils::Castable<FunctionParam, Value> {
  public:
    /// Attributes attached to parameters
    enum class Attribute {
        /// Interpolate attribute
        kInterpolate,
        /// Invariant attribute
        kInvariant,
        /// Location attribute
        kLocation,
        /// Group and binding attributes
        kBindingPoint,
        /// Builtin Vertex index
        kVertexIndex,
        /// Builtin Instance index
        kInstanceIndex,
        /// Builtin Position
        kPosition,
        /// Builtin FrontFacing
        kFrontFacing,
        /// Builtin Local invocation id
        kLocalInvocationId,
        /// Builtin Local invocation index
        kLocalInvocationIndex,
        /// Builtin Global invocation id
        kGlobalInvocationId,
        /// Builtin Workgroup id
        kWorkgroupId,
        /// Builtin Num workgroups
        kNumWorkgroups,
        /// Builtin Sample index
        kSampleIndex,
        /// Builtin Sample mask
        kSampleMask,
    };

    /// Binding information
    struct BindingPoint {
        /// The `@group` part of the binding point
        uint32_t group = 0;
        /// The `@binding` part of the binding point
        uint32_t binding = 0;
    };

    /// Constructor
    /// @param type the type of the var
    explicit FunctionParam(const type::Type* type);
    FunctionParam(const FunctionParam& inst) = delete;
    FunctionParam(FunctionParam&& inst) = delete;
    ~FunctionParam() override;

    FunctionParam& operator=(const FunctionParam& inst) = delete;
    FunctionParam& operator=(FunctionParam&& inst) = delete;

    /// @returns the type of the var
    const type::Type* Type() const override { return type; }

    /// The type of the parameter
    const type::Type* type;

    /// The paramter attributes if any
    utils::Vector<Attribute, 1> attributes;

    /// If the attributes contain `kLocation` this stores the location value.
    std::optional<uint32_t> location;

    /// If the attributes contains `kBindingPoint` this stores the group and binding information
    std::optional<BindingPoint> binding_point;
};

utils::StringStream& operator<<(utils::StringStream& out, FunctionParam::Attribute value);

}  // namespace tint::ir

#endif  // SRC_TINT_IR_FUNCTION_PARAM_H_
