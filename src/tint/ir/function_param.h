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

#include <utility>

#include "src/tint/ir/binding_point.h"
#include "src/tint/ir/value.h"
#include "src/tint/utils/castable.h"
#include "src/tint/utils/vector.h"

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

    /// Constructor
    /// @param type the type of the var
    explicit FunctionParam(const type::Type* type);
    ~FunctionParam() override;

    /// @returns the type of the var
    const type::Type* Type() const override { return type_; }

    /// Sets the parameter attributes
    /// @param attrs the attributes to set
    void SetAttributes(utils::VectorRef<Attribute> attrs) { attributes_ = std::move(attrs); }
    /// @returns the paramter attributes if any
    utils::VectorRef<Attribute> Attributes() const { return attributes_; }

    /// Sets the location
    /// @param loc the location
    void SetLocation(std::optional<uint32_t> loc) { location_ = loc; }
    /// @returns the location if `Attributes` contains `kLocation`
    std::optional<uint32_t> Location() const { return location_; }

    /// Sets the binding point
    /// @param group the group
    /// @param binding the binding
    void SetBindingPoint(uint32_t group, uint32_t binding) { binding_point_ = {group, binding}; }
    /// @returns the binding points if `Attributes` contains `kBindingPoint`
    std::optional<struct BindingPoint> BindingPoint() const { return binding_point_; }

  private:
    const type::Type* type_ = nullptr;
    utils::Vector<Attribute, 1> attributes_;
    std::optional<uint32_t> location_;
    std::optional<struct BindingPoint> binding_point_;
};

utils::StringStream& operator<<(utils::StringStream& out, FunctionParam::Attribute value);

}  // namespace tint::ir

#endif  // SRC_TINT_IR_FUNCTION_PARAM_H_
