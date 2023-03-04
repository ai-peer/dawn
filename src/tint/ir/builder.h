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

#ifndef SRC_TINT_IR_BUILDER_H_
#define SRC_TINT_IR_BUILDER_H_

#include <utility>

#include "src/tint/constant/scalar.h"
#include "src/tint/ir/binary.h"
#include "src/tint/ir/bitcast.h"
#include "src/tint/ir/builtins/abs.h"
#include "src/tint/ir/builtins/acos.h"
#include "src/tint/ir/builtins/acosh.h"
#include "src/tint/ir/builtins/all.h"
#include "src/tint/ir/builtins/any.h"
#include "src/tint/ir/builtins/array_length.h"
#include "src/tint/ir/builtins/asin.h"
#include "src/tint/ir/builtins/asinh.h"
#include "src/tint/ir/builtins/atan.h"
#include "src/tint/ir/builtins/atan2.h"
#include "src/tint/ir/builtins/atanh.h"
#include "src/tint/ir/builtins/atomic_add.h"
#include "src/tint/ir/builtins/atomic_and.h"
#include "src/tint/ir/builtins/atomic_compare_exchange_weak.h"
#include "src/tint/ir/builtins/atomic_exchange.h"
#include "src/tint/ir/builtins/atomic_load.h"
#include "src/tint/ir/builtins/atomic_max.h"
#include "src/tint/ir/builtins/atomic_min.h"
#include "src/tint/ir/builtins/atomic_or.h"
#include "src/tint/ir/builtins/atomic_store.h"
#include "src/tint/ir/builtins/atomic_sub.h"
#include "src/tint/ir/builtins/atomic_xor.h"
#include "src/tint/ir/builtins/ceil.h"
#include "src/tint/ir/builtins/clamp.h"
#include "src/tint/ir/builtins/cos.h"
#include "src/tint/ir/builtins/cosh.h"
#include "src/tint/ir/builtins/count_leading_zeros.h"
#include "src/tint/ir/builtins/count_one_bits.h"
#include "src/tint/ir/builtins/count_trailing_zeros.h"
#include "src/tint/ir/builtins/cross.h"
#include "src/tint/ir/builtins/degrees.h"
#include "src/tint/ir/builtins/determinant.h"
#include "src/tint/ir/builtins/distance.h"
#include "src/tint/ir/builtins/dot.h"
#include "src/tint/ir/builtins/dot4_i8_packed.h"
#include "src/tint/ir/builtins/dot4_u8_packed.h"
#include "src/tint/ir/builtins/dpdx.h"
#include "src/tint/ir/builtins/dpdx_coarse.h"
#include "src/tint/ir/builtins/dpdx_fine.h"
#include "src/tint/ir/builtins/dpdy.h"
#include "src/tint/ir/builtins/dpdy_coarse.h"
#include "src/tint/ir/builtins/dpdy_fine.h"
#include "src/tint/ir/builtins/exp.h"
#include "src/tint/ir/builtins/exp2.h"
#include "src/tint/ir/builtins/extract_bits.h"
#include "src/tint/ir/builtins/face_forward.h"
#include "src/tint/ir/builtins/first_leading_bit.h"
#include "src/tint/ir/builtins/first_trailing_bit.h"
#include "src/tint/ir/builtins/floor.h"
#include "src/tint/ir/builtins/fma.h"
#include "src/tint/ir/builtins/fract.h"
#include "src/tint/ir/builtins/frexp.h"
#include "src/tint/ir/builtins/fwidth.h"
#include "src/tint/ir/builtins/fwidth_coarse.h"
#include "src/tint/ir/builtins/fwidth_fine.h"
#include "src/tint/ir/builtins/insert_bits.h"
#include "src/tint/ir/builtins/inverse_sqrt.h"
#include "src/tint/ir/builtins/ldexp.h"
#include "src/tint/ir/builtins/length.h"
#include "src/tint/ir/builtins/log.h"
#include "src/tint/ir/builtins/log2.h"
#include "src/tint/ir/builtins/max.h"
#include "src/tint/ir/builtins/min.h"
#include "src/tint/ir/builtins/mix.h"
#include "src/tint/ir/builtins/modf.h"
#include "src/tint/ir/builtins/normalize.h"
#include "src/tint/ir/builtins/pack2x16float.h"
#include "src/tint/ir/builtins/pack2x16snorm.h"
#include "src/tint/ir/builtins/pack2x16unorm.h"
#include "src/tint/ir/builtins/pack4x8snorm.h"
#include "src/tint/ir/builtins/pack4x8unorm.h"
#include "src/tint/ir/builtins/pow.h"
#include "src/tint/ir/builtins/quantize_to_f16.h"
#include "src/tint/ir/builtins/radians.h"
#include "src/tint/ir/builtins/reflect.h"
#include "src/tint/ir/builtins/refract.h"
#include "src/tint/ir/builtins/reverse_bits.h"
#include "src/tint/ir/builtins/round.h"
#include "src/tint/ir/builtins/saturate.h"
#include "src/tint/ir/builtins/select.h"
#include "src/tint/ir/builtins/sign.h"
#include "src/tint/ir/builtins/sin.h"
#include "src/tint/ir/builtins/sinh.h"
#include "src/tint/ir/builtins/smoothstep.h"
#include "src/tint/ir/builtins/sqrt.h"
#include "src/tint/ir/builtins/step.h"
#include "src/tint/ir/builtins/storage_barrier.h"
#include "src/tint/ir/builtins/tan.h"
#include "src/tint/ir/builtins/tanh.h"
#include "src/tint/ir/builtins/texture_dimensions.h"
#include "src/tint/ir/builtins/texture_gather.h"
#include "src/tint/ir/builtins/texture_gather_compare.h"
#include "src/tint/ir/builtins/texture_load.h"
#include "src/tint/ir/builtins/texture_num_layers.h"
#include "src/tint/ir/builtins/texture_num_levels.h"
#include "src/tint/ir/builtins/texture_num_samples.h"
#include "src/tint/ir/builtins/texture_sample.h"
#include "src/tint/ir/builtins/texture_sample_base_clamp_to_edge.h"
#include "src/tint/ir/builtins/texture_sample_bias.h"
#include "src/tint/ir/builtins/texture_sample_compare.h"
#include "src/tint/ir/builtins/texture_sample_compare_level.h"
#include "src/tint/ir/builtins/texture_sample_grad.h"
#include "src/tint/ir/builtins/texture_sample_level.h"
#include "src/tint/ir/builtins/texture_store.h"
#include "src/tint/ir/builtins/transpose.h"
#include "src/tint/ir/builtins/trunc.h"
#include "src/tint/ir/builtins/unpack2x16float.h"
#include "src/tint/ir/builtins/unpack2x16snorm.h"
#include "src/tint/ir/builtins/unpack2x16unorm.h"
#include "src/tint/ir/builtins/unpack4x8snorm.h"
#include "src/tint/ir/builtins/unpack4x8unorm.h"
#include "src/tint/ir/builtins/workgroup_barrier.h"
#include "src/tint/ir/builtins/workgroup_uniform_load.h"
#include "src/tint/ir/constant.h"
#include "src/tint/ir/function.h"
#include "src/tint/ir/if.h"
#include "src/tint/ir/loop.h"
#include "src/tint/ir/module.h"
#include "src/tint/ir/switch.h"
#include "src/tint/ir/temp.h"
#include "src/tint/ir/terminator.h"
#include "src/tint/ir/user_call.h"
#include "src/tint/ir/value.h"
#include "src/tint/ir/value_constructor.h"
#include "src/tint/ir/value_conversion.h"
#include "src/tint/type/bool.h"
#include "src/tint/type/f16.h"
#include "src/tint/type/f32.h"
#include "src/tint/type/i32.h"
#include "src/tint/type/u32.h"

namespace tint::ir {

/// Builds an ir::Module
class Builder {
  public:
    /// Constructor
    Builder();
    /// Constructor
    /// @param mod the ir::Module to wrap with this builder
    explicit Builder(Module&& mod);
    /// Destructor
    ~Builder();

    /// @returns a new block flow node
    Block* CreateBlock();

    /// @returns a new terminator flow node
    Terminator* CreateTerminator();

    /// Creates a function flow node
    /// @returns the flow node
    Function* CreateFunction();

    /// Creates an if flow node
    /// @returns the flow node
    If* CreateIf();

    /// Creates a loop flow node
    /// @returns the flow node
    Loop* CreateLoop();

    /// Creates a switch flow node
    /// @returns the flow node
    Switch* CreateSwitch();

    /// Creates a case flow node for the given case branch.
    /// @param s the switch to create the case into
    /// @param selectors the case selectors for the case statement
    /// @returns the start block for the case flow node
    Block* CreateCase(Switch* s, utils::VectorRef<Switch::CaseSelector> selectors);

    /// Branches the given block to the given flow node.
    /// @param from the block to branch from
    /// @param to the node to branch too
    /// @param args arguments to the branch
    void Branch(Block* from, FlowNode* to, utils::VectorRef<Value*> args);

    /// Creates a constant::Value
    /// @param args the arguments
    /// @returns the new constant value
    template <typename T, typename... ARGS>
    traits::EnableIf<traits::IsTypeOrDerived<T, constant::Value>, const T>* create(ARGS&&... args) {
        return ir.constants.Create<T>(std::forward<ARGS>(args)...);
    }

    /// Creates a new ir::Constant
    /// @param val the constant value
    /// @returns the new constant
    ir::Constant* Constant(const constant::Value* val) {
        return ir.values.Create<ir::Constant>(val);
    }

    /// Creates a ir::Constant for an i32 Scalar
    /// @param v the value
    /// @returns the new constant
    ir::Constant* Constant(i32 v) {
        return Constant(create<constant::Scalar<i32>>(ir.types.Get<type::I32>(), v));
    }

    /// Creates a ir::Constant for a u32 Scalar
    /// @param v the value
    /// @returns the new constant
    ir::Constant* Constant(u32 v) {
        return Constant(create<constant::Scalar<u32>>(ir.types.Get<type::U32>(), v));
    }

    /// Creates a ir::Constant for a f32 Scalar
    /// @param v the value
    /// @returns the new constant
    ir::Constant* Constant(f32 v) {
        return Constant(create<constant::Scalar<f32>>(ir.types.Get<type::F32>(), v));
    }

    /// Creates a ir::Constant for a f16 Scalar
    /// @param v the value
    /// @returns the new constant
    ir::Constant* Constant(f16 v) {
        return Constant(create<constant::Scalar<f16>>(ir.types.Get<type::F16>(), v));
    }

    /// Creates a ir::Constant for a bool Scalar
    /// @param v the value
    /// @returns the new constant
    ir::Constant* Constant(bool v) {
        return Constant(create<constant::Scalar<bool>>(ir.types.Get<type::Bool>(), v));
    }

    /// Creates a new Temporary
    /// @param type the type of the temporary
    /// @returns the new temporary
    ir::Temp* Temp(const type::Type* type) {
        return ir.values.Create<ir::Temp>(type, AllocateTempId());
    }

    /// Creates an op for `lhs kind rhs`
    /// @param kind the kind of operation
    /// @param type the result type of the binary expression
    /// @param lhs the left-hand-side of the operation
    /// @param rhs the right-hand-side of the operation
    /// @returns the operation
    Binary* CreateBinary(Binary::Kind kind, const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an And operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* And(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Or operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Or(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Xor operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Xor(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an LogicalAnd operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* LogicalAnd(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an LogicalOr operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* LogicalOr(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Equal operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Equal(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an NotEqual operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* NotEqual(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an LessThan operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* LessThan(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an GreaterThan operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* GreaterThan(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an LessThanEqual operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* LessThanEqual(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an GreaterThanEqual operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* GreaterThanEqual(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an ShiftLeft operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* ShiftLeft(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an ShiftRight operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* ShiftRight(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Add operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Add(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Subtract operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Subtract(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Multiply operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Multiply(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Divide operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Divide(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates an Modulo operation
    /// @param type the result type of the expression
    /// @param lhs the lhs of the add
    /// @param rhs the rhs of the add
    /// @returns the operation
    Binary* Modulo(const type::Type* type, Value* lhs, Value* rhs);

    /// Creates a bitcast instruction
    /// @param type the result type of the bitcast
    /// @param val the value being bitcast
    /// @returns the instruction
    ir::Bitcast* Bitcast(const type::Type* type, Value* val);

    /// Creates a user function call instruction
    /// @param type the return type of the call
    /// @param name the name of the function being called
    /// @param args the call arguments
    /// @returns the instruction
    ir::UserCall* UserCall(const type::Type* type, Symbol name, utils::VectorRef<Value*> args);

    /// Creates a value conversion instruction
    /// @param to the type converted to
    /// @param from the type converted from
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::ValueConversion* ValueConversion(const type::Type* to,
                                         const type::Type* from,
                                         utils::VectorRef<Value*> args);

    /// Creates a value constructor instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::ValueConstructor* ValueConstructor(const type::Type* to, utils::VectorRef<Value*> args);

    /// Creates a Abs instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Abs* Abs(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Acos instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Acos* Acos(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Acosh instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Acosh* Acosh(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a All instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::All* All(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Any instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Any* Any(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a ArrayLength instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::ArrayLength* ArrayLength(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Asin instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Asin* Asin(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Asinh instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Asinh* Asinh(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Atan instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Atan* Atan(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Atan2 instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Atan2* Atan2(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Atanh instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Atanh* Atanh(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Ceil instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Ceil* Ceil(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Clamp instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Clamp* Clamp(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Cos instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Cos* Cos(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Cosh instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Cosh* Cosh(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a CountLeadingZeros instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::CountLeadingZeros* CountLeadingZeros(const type::Type* to,
                                                       utils::VectorRef<Value*> args);
    /// Creates a CountOneBits instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::CountOneBits* CountOneBits(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a CountTrailingZeros instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::CountTrailingZeros* CountTrailingZeros(const type::Type* to,
                                                         utils::VectorRef<Value*> args);
    /// Creates a Cross instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Cross* Cross(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Degrees instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Degrees* Degrees(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Determinant instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Determinant* Determinant(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Distance instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Distance* Distance(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Dot instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Dot* Dot(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a value constructor instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Dot4I8Packed* Dot4I8Packed(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Dot4I8Packed instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Dot4U8Packed* Dot4U8Packed(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Dpdx instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Dpdx* Dpdx(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a DpdxCoarse instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::DpdxCoarse* DpdxCoarse(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a DpdxFine instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::DpdxFine* DpdxFine(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Dpdy instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Dpdy* Dpdy(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a DpdyCoarse instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::DpdyCoarse* DpdyCoarse(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a DpdyFine instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::DpdyFine* DpdyFine(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Exp instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Exp* Exp(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Exp2 instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Exp2* Exp2(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a ExtractBits instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::ExtractBits* ExtractBits(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a FaceForward instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::FaceForward* FaceForward(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a FirstLeadingBit instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::FirstLeadingBit* FirstLeadingBit(const type::Type* to,
                                                   utils::VectorRef<Value*> args);
    /// Creates a FirstTrailingBit instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::FirstTrailingBit* FirstTrailingBit(const type::Type* to,
                                                     utils::VectorRef<Value*> args);
    /// Creates a Floor instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Floor* Floor(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Fma instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Fma* Fma(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Fract instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Fract* Fract(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Frexp instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Frexp* Frexp(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Fwidth instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Fwidth* Fwidth(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a FwidthCoarse instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::FwidthCoarse* FwidthCoarse(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a FwidthFine instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::FwidthFine* FwidthFine(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a InsertBits instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::InsertBits* InsertBits(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a InverseSqrt instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::InverseSqrt* InverseSqrt(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Ldexp instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Ldexp* Ldexp(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Length instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Length* Length(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Log instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Log* Log(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Log2 instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Log2* Log2(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Max instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Max* Max(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Min instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Min* Min(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Mix instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Mix* Mix(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Modf instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Modf* Modf(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Normalize instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Normalize* Normalize(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Pack2X16Float instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Pack2X16Float* Pack2X16Float(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Pack2X16Snorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Pack2X16Snorm* Pack2X16Snorm(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Pack2X16Unorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Pack2X16Unorm* Pack2X16Unorm(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Pack4X8Snorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Pack4X8Snorm* Pack4X8Snorm(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Pack4X8Unorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Pack4X8Unorm* Pack4X8Unorm(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Pow instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Pow* Pow(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a QuantizeToF16 instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::QuantizeToF16* QuantizeToF16(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Radians instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Radians* Radians(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Reflect instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Reflect* Reflect(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Refract instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Refract* Refract(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a ReverseBits instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::ReverseBits* ReverseBits(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Round instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Round* Round(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Saturate instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Saturate* Saturate(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Select instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Select* Select(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Sign instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Sign* Sign(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Sin instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Sin* Sin(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Sinh instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Sinh* Sinh(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Smoothstep instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Smoothstep* Smoothstep(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Sqrt instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Sqrt* Sqrt(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Step instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Step* Step(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a StorageBarrier instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::StorageBarrier* StorageBarrier(const type::Type* to,
                                                 utils::VectorRef<Value*> args);
    /// Creates a Tan instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Tan* Tan(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Tanh instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Tanh* Tanh(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Transpose instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Transpose* Transpose(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Trunc instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Trunc* Trunc(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a Unpack2X16Float instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Unpack2X16Float* Unpack2X16Float(const type::Type* to,
                                                   utils::VectorRef<Value*> args);
    /// Creates a Unpack2X16Snorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Unpack2X16Snorm* Unpack2X16Snorm(const type::Type* to,
                                                   utils::VectorRef<Value*> args);
    /// Creates a Unpack2X16Unorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Unpack2X16Unorm* Unpack2X16Unorm(const type::Type* to,
                                                   utils::VectorRef<Value*> args);
    /// Creates a Unpack4X8Snorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Unpack4X8Snorm* Unpack4X8Snorm(const type::Type* to,
                                                 utils::VectorRef<Value*> args);
    /// Creates a Unpack4X8Unorm instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::Unpack4X8Unorm* Unpack4X8Unorm(const type::Type* to,
                                                 utils::VectorRef<Value*> args);
    /// Creates a WorkgroupBarrier instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::WorkgroupBarrier* WorkgroupBarrier(const type::Type* to,
                                                     utils::VectorRef<Value*> args);
    /// Creates a WorkgroupUniformLoad instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::WorkgroupUniformLoad* WorkgroupUniformLoad(const type::Type* to,
                                                             utils::VectorRef<Value*> args);
    /// Creates a TextureDimensions instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureDimensions* TextureDimensions(const type::Type* to,
                                                       utils::VectorRef<Value*> args);
    /// Creates a TextureGather instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureGather* TextureGather(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a TextureGatherCompare instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureGatherCompare* TextureGatherCompare(const type::Type* to,
                                                             utils::VectorRef<Value*> args);
    /// Creates a TextureNumLayers instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureNumLayers* TextureNumLayers(const type::Type* to,
                                                     utils::VectorRef<Value*> args);
    /// Creates a TextureNumLevels instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureNumLevels* TextureNumLevels(const type::Type* to,
                                                     utils::VectorRef<Value*> args);
    /// Creates a TextureNumSamples instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureNumSamples* TextureNumSamples(const type::Type* to,
                                                       utils::VectorRef<Value*> args);
    /// Creates a TextureSample instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSample* TextureSample(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a TextureSampleBias instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSampleBias* TextureSampleBias(const type::Type* to,
                                                       utils::VectorRef<Value*> args);
    /// Creates a TextureSampleCompare instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSampleCompare* TextureSampleCompare(const type::Type* to,
                                                             utils::VectorRef<Value*> args);
    /// Creates a TextureSampleCompareLevel instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSampleCompareLevel* TextureSampleCompareLevel(
        const type::Type* to,
        utils::VectorRef<Value*> args);
    /// Creates a TextureSampleGrad instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSampleGrad* TextureSampleGrad(const type::Type* to,
                                                       utils::VectorRef<Value*> args);
    /// Creates a TextureSampleLevel instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSampleLevel* TextureSampleLevel(const type::Type* to,
                                                         utils::VectorRef<Value*> args);
    /// Creates a TextureSampleBaseClampToEdge instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureSampleBaseClampToEdge* TextureSampleBaseClampToEdge(
        const type::Type* to,
        utils::VectorRef<Value*> args);
    /// Creates a TextureStore instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureStore* TextureStore(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a TextureLoad instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::TextureLoad* TextureLoad(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicLoad instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicLoad* AtomicLoad(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicStore instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicStore* AtomicStore(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicAdd instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicAdd* AtomicAdd(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicSub instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicSub* AtomicSub(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicMax instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicMax* AtomicMax(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicMin instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicMin* AtomicMin(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicAnd instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicAnd* AtomicAnd(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicOr instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicOr* AtomicOr(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicXor instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicXor* AtomicXor(const type::Type* to, utils::VectorRef<Value*> args);
    /// Creates a AtomicExchangeinstruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicExchange* AtomicExchange(const type::Type* to,
                                                 utils::VectorRef<Value*> args);
    /// Creates a AtomicCompareExchangeWeak instruction
    /// @param to the type being converted
    /// @param args the arguments to be converted
    /// @returns the instruction
    ir::builtins::AtomicCompareExchangeWeak* AtomicCompareExchangeWeak(
        const type::Type* to,
        utils::VectorRef<Value*> args);

    /// @returns a unique temp id
    Temp::Id AllocateTempId();

    /// The IR module.
    Module ir;

    /// The next temporary number to allocate
    Temp::Id next_temp_id = 1;
};

}  // namespace tint::ir

#endif  // SRC_TINT_IR_BUILDER_H_
