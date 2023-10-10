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

#include <iostream>

#include "src/tint/cmd/fuzz/ir/ir_fuzz.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/validator.h"
#include "src/tint/utils/bytes/reader.h"
#include "src/tint/utils/cli/cli.h"
#include "src/tint/utils/result/result.h"
#include "src/tint/utils/text/unicode.h"

using namespace tint::core::fluent_types;     // NOLINT
using namespace tint::core::number_suffixes;  // NOLINT

namespace tint {
namespace {

class Builder {
  public:
    Builder(bytes::Reader reader) : r(reader) {}
    Result<core::ir::Module> Build() {
        while (!r.IsEOF()) {
            if (!TaggedDispatch([&] { return AddToRootBlock(); },  //
                                [&] { return Function(); },        //
                                [&] { return PopulateBlock(); })) {
                return Failure{};
            }
        }
        return std::move(ir);
    }

  private:
    using FnID = uint8_t;
    using ValueID = uint16_t;
    using PtrID = uint8_t;
    using ArrayID = uint8_t;
    using StructID = uint8_t;
    using IfID = uint8_t;
    using SwitchID = uint8_t;
    using LoopID = uint8_t;
    using BlockID = uint8_t;

    bytes::Reader r;
    core::ir::Module ir{};
    core::ir::Builder b{ir};
    Hashmap<FnID, core::ir::Function*, 8> fns_{};
    Hashmap<ValueID, core::ir::Value*, 256> values_{};
    Hashmap<PtrID, const core::type::Pointer*, 8> ptrs_{};
    Hashmap<ArrayID, const core::type::Array*, 8> arrays_{};
    Hashmap<StructID, const core::type::Struct*, 8> structs_{};
    Hashmap<IfID, core::ir::If*, 8> ifs_{};
    Hashmap<SwitchID, core::ir::Switch*, 8> switches_{};
    Hashmap<LoopID, core::ir::Loop*, 8> loops_{};
    Hashmap<BlockID, core::ir::Block*, 8> blocks_{};

    template <typename... FNS>
    [[nodiscard]] bool TaggedDispatch(FNS&&... fns) {
        Vector<std::function<bool()>, sizeof...(fns)> funcs{fns...};
        while (!r.IsEOF()) {
            uint8_t i = r.Int<uint8_t>();
            if (i == 0) {
                return true;
            }
            if (i >= funcs.Length()) {
                return false;
            }
            if (!funcs[i]()) {
                return false;
            }
        }
        return false;
    }

    template <typename ENUM>
    Result<ENUM> Enum(std::initializer_list<ENUM> values) {
        auto i = r.Int<uint8_t>();
        if (i >= values.size()) {
            return Failure{};
        }
        return values.begin()[i];
    }

    bool AddToRootBlock() {
        auto id = r.Int<ValueID>();
        auto ptr = Ptr();
        if (!ptr) {
            return false;
        }
        auto var = b.ir.root_block->Append(b.Var(ptr));
        if (!values_.Add(id, var->Result())) {
            return false;
        }
        return true;
    }

    core::ir::Function* Function() {
        auto* fn = fns_.GetOrCreate(r.Int<FnID>(), [&]() -> core::ir::Function* {
            auto name = IdentString(3);
            if (name.empty()) {
                return nullptr;
            }
            auto ret_ty = Type();
            if (!ret_ty) {
                return nullptr;
            }
            auto stage = core::ir::Function::PipelineStage::kUndefined;
            std::optional<std::array<uint32_t, 3UL>> workgroup_size;
            Vector<core::ir::FunctionParam*, 4> params;
            if (!TaggedDispatch(
                    [&] {
                        auto s = PipelineStage();
                        if (!s) {
                            return false;
                        }
                        stage = s.Get();
                        return true;
                    },
                    [&] {
                        workgroup_size = WorkgroupSize();
                        return true;
                    },
                    [&] {
                        auto param = Parameter();
                        if (!param) {
                            return false;
                        }
                        params.Push(param);
                        return true;
                    })) {
                return nullptr;
            }
            auto* fn = b.Function(name, ret_ty, stage, workgroup_size);
            fn->SetParams(std::move(params));
            return fn;
        });

        if (!fn) {
            return nullptr;
        }

        if (!RegisterAndPopulateBlock(fn->Block())) {
            return nullptr;
        }

        return fn;
    }

    core::ir::FunctionParam* Parameter() {
        auto name = IdentString(3);
        if (name.empty()) {
            return nullptr;
        }
        auto ty = Type();
        if (!ty) {
            return nullptr;
        }
        return b.FunctionParam(name, ty);
    }

    bool PopulateBlock() {
        auto block = blocks_.Get(r.Int<BlockID>());
        if (!block) {
            return false;
        }
        return PopulateBlock(block.value());
    }

    bool RegisterAndPopulateBlock(core::ir::Block* block) {
        if (!blocks_.Add(r.Int<BlockID>(), block)) {
            return false;
        }
        return PopulateBlock(block);
    }

    bool PopulateBlock(core::ir::Block* block) {
        return TaggedDispatch(
            [&] {
                auto inst = Instruction();
                if (!inst) {
                    return false;
                }
                if (auto* t = block->Terminator()) {
                    inst->InsertBefore(t);
                } else {
                    block->Append(inst);
                }
                return true;
            },
            [&] {
                auto inst = Instruction();
                if (!inst) {
                    return false;
                }
                block->Prepend(inst);
                return true;
            });
    }

    core::ir::Instruction* Instruction() {
        core::ir::Instruction* inst = nullptr;
        switch (r.Int<uint8_t>()) {
            case 0:
                inst = Return();
                break;
            case 20:
                inst = If();
                break;
            case 21:
                inst = ExitIf();
                break;
            case 22:
                inst = Switch();
                break;
            case 23:
                inst = ExitSwitch();
                break;
            case 24:
                inst = Loop();
                break;
            case 25:
                inst = ExitLoop();
                break;
            case 50:
                inst = Unary();
                break;
            case 51:
                inst = Binary();
                break;
            case 52:
                inst = BuiltinCall();
                break;
            case 53:
                inst = Call();
                break;
            case 60:
                inst = Let();
                break;
            default:
                return nullptr;
        }

        if (!inst) {
            return nullptr;
        }

        if (!RegisterResults(inst)) {
            return nullptr;
        }

        return inst;
    }

    bool RegisterResults(core::ir::Instruction* inst) {
        for (auto* res : inst->Results()) {
            if (!values_.Add(r.Int<ValueID>(), res)) {
                return false;
            }
        }
        return true;
    }

    core::ir::Return* Return() {
        auto id = r.Int<FnID>();
        auto fn = fns_.Get(id);
        if (!fn) {
            return nullptr;
        }

        if (!r.Bool()) {
            return b.Return(fn.value());
        }

        auto val = Value();
        if (!val) {
            return nullptr;
        }
        return b.Return(fn.value(), val);
    }

    core::ir::Unary* Unary() {
        auto op = UnaryOp();
        if (!op) {
            return nullptr;
        }
        auto ty = Type();
        if (!ty) {
            return nullptr;
        }
        auto val = Value();
        if (!val) {
            return nullptr;
        }
        return b.Unary(op.Get(), ty, val);
    }

    core::ir::Binary* Binary() {
        auto op = BinaryOp();
        if (!op) {
            return nullptr;
        }
        auto ty = Type();
        if (!ty) {
            return nullptr;
        }
        auto lhs = Value();
        if (!lhs) {
            return nullptr;
        }
        auto rhs = Value();
        if (!rhs) {
            return nullptr;
        }
        return b.Binary(op.Get(), ty, lhs, rhs);
    }

    core::ir::BuiltinCall* BuiltinCall() {
        auto fn = BuiltinFn();
        if (!fn) {
            return nullptr;
        }
        auto* ty = Type();
        if (!ty) {
            return nullptr;
        }
        auto num_args = r.Int<uint8_t>();
        if (num_args > 8) {
            return nullptr;
        }
        Vector<core::ir::Value*, 8> args;
        for (uint8_t i = 0; i < num_args; i++) {
            auto arg = Value();
            if (!arg) {
                return nullptr;
            }
            args.Push(arg);
        }
        return b.Call(ty, fn.Get(), std::move(args));
    }

    core::ir::Call* Call() {
        auto fn = Function();
        if (!fn) {
            return nullptr;
        }
        auto num_args = r.Int<uint8_t>();
        Vector<core::ir::Value*, 8> args;
        args.Reserve(num_args);
        for (uint8_t i = 0; i < num_args; i++) {
            auto arg = Value();
            if (!arg) {
                return nullptr;
            }
            args.Push(arg);
        }
        return b.Call(fn, std::move(args));
    }

    core::ir::Let* Let() {
        auto name = IdentString(3);
        if (name.empty()) {
            return nullptr;
        }
        auto val = Value();
        if (!val) {
            return nullptr;
        }
        return b.Let(name, val);
    }

    core::ir::If* If() {
        auto* cond = Value();
        if (!cond) {
            return nullptr;
        }

        auto* if_ = b.If(cond);
        if (!ifs_.Add(r.Int<IfID>(), if_)) {
            return nullptr;
        }

        auto results = InstructionResults(4);
        if (!results) {
            return nullptr;
        }
        if_->SetResults(std::move(results.Get()));

        if (!RegisterAndPopulateBlock(if_->True()) || !RegisterAndPopulateBlock(if_->False())) {
            return nullptr;
        }

        return if_;
    }

    core::ir::ExitIf* ExitIf() {
        auto id = r.Int<IfID>();
        auto if_ = ifs_.Get(id);
        if (!if_) {
            return nullptr;
        }
        auto vals = Values(4);
        if (!vals) {
            return nullptr;
        }
        return b.ExitIf(if_.value(), std::move(vals.Get()));
    }

    core::ir::Switch* Switch() {
        auto* cond = Value();
        if (!cond) {
            return nullptr;
        }

        auto* switch_ = b.Switch(cond);
        if (!switches_.Add(r.Int<SwitchID>(), switch_)) {
            return nullptr;
        }

        auto results = InstructionResults(4);
        if (!results) {
            return nullptr;
        }
        switch_->SetResults(std::move(results.Get()));

        auto num_cases = r.Int<uint8_t>();
        if (num_cases > 8) {
            return nullptr;
        }

        for (size_t case_idx = 0; case_idx < num_cases; case_idx++) {
            auto num_sels = r.Int<uint8_t>();
            if (num_sels > 4) {
                return nullptr;
            }
            Vector<core::ir::Switch::CaseSelector, 4> sels;
            for (size_t sel_idx = 0; sel_idx < num_sels; sel_idx++) {
                sels.Push(core::ir::Switch::CaseSelector{ConstantInt()});
            }

            auto* case_ = b.Case(switch_, std::move(sels));
            if (!RegisterAndPopulateBlock(case_)) {
                return nullptr;
            }
        }

        return switch_;
    }

    core::ir::ExitSwitch* ExitSwitch() {
        auto id = r.Int<SwitchID>();
        auto switch_ = switches_.Get(id);
        if (!switch_) {
            return nullptr;
        }
        auto vals = Values(4);
        if (!vals) {
            return nullptr;
        }
        return b.ExitSwitch(switch_.value(), std::move(vals.Get()));
    }

    core::ir::Loop* Loop() {
        auto* loop = b.Loop();
        if (!loops_.Add(r.Int<LoopID>(), loop)) {
            return nullptr;
        }

        auto results = InstructionResults(4);
        if (!results) {
            return nullptr;
        }
        loop->SetResults(std::move(results.Get()));

        if (!RegisterAndPopulateBlock(loop->Initializer())) {
            return nullptr;
        }

        if (!RegisterAndPopulateBlock(loop->Body())) {
            return nullptr;
        }

        if (!RegisterAndPopulateBlock(loop->Continuing())) {
            return nullptr;
        }

        return loop;
    }

    core::ir::ExitLoop* ExitLoop() {
        auto id = r.Int<LoopID>();
        auto loop_ = loops_.Get(id);
        if (!loop_) {
            return nullptr;
        }
        auto vals = Values(4);
        if (!vals) {
            return nullptr;
        }
        return b.ExitLoop(loop_.value(), std::move(vals.Get()));
    }

    Result<Vector<core::ir::InstructionResult*, 8>> InstructionResults(size_t max) {
        auto num_results = r.Int<uint8_t>();
        if (num_results > max) {
            return Failure{};
        }

        Vector<core::ir::InstructionResult*, 8> results;
        results.Reserve(num_results);
        for (uint8_t i = 0; i < num_results; i++) {
            auto res = InstructionResult();
            if (!res) {
                return Failure{};
            }
            results.Push(res);
        }

        return results;
    }

    core::ir::InstructionResult* InstructionResult() {
        auto ty = Type();
        if (!ty) {
            return nullptr;
        }
        return b.InstructionResult(ty);
    }

    Result<Vector<core::ir::Value*, 8>> Values(size_t max) {
        auto num = r.Int<uint8_t>();
        if (num > max) {
            return Failure{};
        }

        Vector<core::ir::Value*, 8> values;
        values.Reserve(num);
        for (uint8_t i = 0; i < num; i++) {
            auto res = Value();
            if (!res) {
                return Failure{};
            }
            values.Push(res);
        }

        return values;
    }

    core::ir::Value* Value() {
        switch (r.Int<uint8_t>()) {
            case 0: {
                auto id = r.Int<ValueID>();
                return *values_.GetOrZero(id);
            }
            case 10:
                return b.Constant(r.Bool());
            case 11:
                return b.Constant(i32(r.Int<int32_t>()));
            case 12:
                return b.Constant(u32(r.Int<uint32_t>()));
            case 13: {
                float f = r.Float<float>();
                if (!std::isfinite(f)) {
                    return nullptr;
                }
                return b.Constant(f32(f));
            }
            case 14: {
                float f = f16::FromBits(r.Int<uint16_t>());
                if (!std::isfinite(f)) {
                    return nullptr;
                }
                return b.Constant(f16(f));
            }
        }
        return nullptr;
    }

    core::ir::Constant* ConstantInt() {
        switch (r.Int<uint8_t>()) {
            case 0:
                return b.Constant(i32(r.Int<int32_t>()));
            case 1:
                return b.Constant(u32(r.Int<uint32_t>()));
        }
        return nullptr;
    }

    const core::type::Type* Type() {
        switch (r.Int<uint8_t>()) {
            case 0: {
                return b.ir.Types().void_();
            }
            case 1: {
                return b.ir.Types().bool_();
            }
            case 2: {
                return b.ir.Types().i32();
            }
            case 3: {
                return b.ir.Types().u32();
            }
            case 4: {
                return b.ir.Types().f32();
            }
            case 5: {
                return b.ir.Types().f16();
            }
            case 10: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().vec2(scalar) : nullptr;
            }
            case 11: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().vec3(scalar) : nullptr;
            }
            case 12: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().vec4(scalar) : nullptr;
            }
            case 20: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat2x2(scalar) : nullptr;
            }
            case 21: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat2x3(scalar) : nullptr;
            }
            case 22: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat2x4(scalar) : nullptr;
            }
            case 23: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat3x2(scalar) : nullptr;
            }
            case 24: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat3x3(scalar) : nullptr;
            }
            case 25: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat3x4(scalar) : nullptr;
            }
            case 26: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat4x2(scalar) : nullptr;
            }
            case 27: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat4x3(scalar) : nullptr;
            }
            case 28: {
                auto scalar = Scalar();
                return scalar ? b.ir.Types().mat4x4(scalar) : nullptr;
            }
            case 51: {
                return Ptr();
            }
            case 52: {
                return Array();
            }
            case 53: {
                return Struct();
            }
        }
        return nullptr;
    }

    const core::type::Scalar* Scalar() {
        switch (r.Int<uint8_t>()) {
            case 0:
                return b.ir.Types().bool_();
            case 1:
                return b.ir.Types().i32();
            case 2:
                return b.ir.Types().u32();
            case 3:
                return b.ir.Types().f32();
            case 4:
                return b.ir.Types().f16();
        }
        return nullptr;
    }

    const core::type::Pointer* Ptr() {
        return ptrs_.GetOrCreate(r.Int<PtrID>(), [&]() -> const core::type::Pointer* {
            auto address_space = AddressSpace();
            if (!address_space) {
                return nullptr;
            }
            auto access = Access();
            if (!access) {
                return nullptr;
            }
            auto ty = Type();
            if (!ty) {
                return nullptr;
            }
            return b.ir.Types().ptr(address_space.Get(), ty, access.Get());
        });
    }

    const core::type::Array* Array() {
        return arrays_.GetOrCreate(r.Int<ArrayID>(), [&]() -> const core::type::Array* {
            auto ty = Type();
            if (!ty || ty->Size() == 0) {
                return nullptr;
            }
            uint32_t n = r.Int<uint32_t>();
            return b.ir.Types().array(ty, n);
        });
    }

    const core::type::Struct* Struct() {
        return structs_.GetOrCreate(r.Int<StructID>(), [&]() -> const core::type::Struct* {
            auto name = IdentString(3);
            if (name.empty()) {
                return nullptr;
            }
            Vector<core::type::Manager::StructMemberDesc, 4> members;
            members.Resize(r.Int<uint8_t>());
            for (auto& member : members) {
                auto desc = StructMemberDesc();
                if (!desc) {
                    return nullptr;
                }
                member = desc.Get();
            }
            if (members.IsEmpty()) {
                return nullptr;
            }
            return b.ir.Types().Struct(b.ir.symbols.Register(name), members);
        });
    }

    Result<core::type::Manager::StructMemberDesc> StructMemberDesc() {
        auto name = IdentString(3);
        if (name.empty()) {
            return Failure{};
        }
        core::type::Manager::StructMemberDesc out;
        out.name = b.ir.symbols.Register(name);
        out.type = b.ir.Types().i32();
        if (!TaggedDispatch(
                [&] {
                    out.type = Type();
                    return out.type != nullptr && out.type->Size() != 0;
                },
                [&] {
                    out.attributes.location = r.Int<uint32_t>();
                    return true;
                },
                [&] {
                    out.attributes.index = r.Int<uint32_t>();
                    return true;
                },
                [&] {
                    auto builtin = BuiltinValue();
                    if (!builtin) {
                        return false;
                    }
                    out.attributes.builtin = builtin.Get();
                    return true;
                },
                [&] {
                    auto interpolation = Interpolation();
                    if (!interpolation) {
                        return false;
                    }
                    out.attributes.interpolation = interpolation.Get();
                    return true;
                },
                [&] {
                    out.attributes.invariant = r.Bool();
                    return true;
                })) {
            return Failure{};
        }

        return out;
    }

    Result<core::ir::Function::PipelineStage> PipelineStage() {
        return Enum({
            core::ir::Function::PipelineStage::kCompute,
            core::ir::Function::PipelineStage::kFragment,
            core::ir::Function::PipelineStage::kVertex,
        });
    }

    Result<core::BuiltinFn> BuiltinFn() {
        return Enum({
            core::BuiltinFn::kAbs,
            core::BuiltinFn::kAcos,
            core::BuiltinFn::kAcosh,
            core::BuiltinFn::kAll,
            core::BuiltinFn::kAny,
            core::BuiltinFn::kArrayLength,
            core::BuiltinFn::kAsin,
            core::BuiltinFn::kAsinh,
            core::BuiltinFn::kAtan,
            core::BuiltinFn::kAtan2,
            core::BuiltinFn::kAtanh,
            core::BuiltinFn::kCeil,
            core::BuiltinFn::kClamp,
            core::BuiltinFn::kCos,
            core::BuiltinFn::kCosh,
            core::BuiltinFn::kCountLeadingZeros,
            core::BuiltinFn::kCountOneBits,
            core::BuiltinFn::kCountTrailingZeros,
            core::BuiltinFn::kCross,
            core::BuiltinFn::kDegrees,
            core::BuiltinFn::kDeterminant,
            core::BuiltinFn::kDistance,
            core::BuiltinFn::kDot,
            core::BuiltinFn::kDot4I8Packed,
            core::BuiltinFn::kDot4U8Packed,
            core::BuiltinFn::kDpdx,
            core::BuiltinFn::kDpdxCoarse,
            core::BuiltinFn::kDpdxFine,
            core::BuiltinFn::kDpdy,
            core::BuiltinFn::kDpdyCoarse,
            core::BuiltinFn::kDpdyFine,
            core::BuiltinFn::kExp,
            core::BuiltinFn::kExp2,
            core::BuiltinFn::kExtractBits,
            core::BuiltinFn::kFaceForward,
            core::BuiltinFn::kFirstLeadingBit,
            core::BuiltinFn::kFirstTrailingBit,
            core::BuiltinFn::kFloor,
            core::BuiltinFn::kFma,
            core::BuiltinFn::kFract,
            core::BuiltinFn::kFrexp,
            core::BuiltinFn::kFwidth,
            core::BuiltinFn::kFwidthCoarse,
            core::BuiltinFn::kFwidthFine,
            core::BuiltinFn::kInsertBits,
            core::BuiltinFn::kInverseSqrt,
            core::BuiltinFn::kLdexp,
            core::BuiltinFn::kLength,
            core::BuiltinFn::kLog,
            core::BuiltinFn::kLog2,
            core::BuiltinFn::kMax,
            core::BuiltinFn::kMin,
            core::BuiltinFn::kMix,
            core::BuiltinFn::kModf,
            core::BuiltinFn::kNormalize,
            core::BuiltinFn::kPack2X16Float,
            core::BuiltinFn::kPack2X16Snorm,
            core::BuiltinFn::kPack2X16Unorm,
            core::BuiltinFn::kPack4X8Snorm,
            core::BuiltinFn::kPack4X8Unorm,
            core::BuiltinFn::kPow,
            core::BuiltinFn::kQuantizeToF16,
            core::BuiltinFn::kRadians,
            core::BuiltinFn::kReflect,
            core::BuiltinFn::kRefract,
            core::BuiltinFn::kReverseBits,
            core::BuiltinFn::kRound,
            core::BuiltinFn::kSaturate,
            core::BuiltinFn::kSelect,
            core::BuiltinFn::kSign,
            core::BuiltinFn::kSin,
            core::BuiltinFn::kSinh,
            core::BuiltinFn::kSmoothstep,
            core::BuiltinFn::kSqrt,
            core::BuiltinFn::kStep,
            core::BuiltinFn::kStorageBarrier,
            core::BuiltinFn::kTan,
            core::BuiltinFn::kTanh,
            core::BuiltinFn::kTranspose,
            core::BuiltinFn::kTrunc,
            core::BuiltinFn::kUnpack2X16Float,
            core::BuiltinFn::kUnpack2X16Snorm,
            core::BuiltinFn::kUnpack2X16Unorm,
            core::BuiltinFn::kUnpack4X8Snorm,
            core::BuiltinFn::kUnpack4X8Unorm,
            core::BuiltinFn::kWorkgroupBarrier,
            core::BuiltinFn::kTextureBarrier,
            core::BuiltinFn::kTextureDimensions,
            core::BuiltinFn::kTextureGather,
            core::BuiltinFn::kTextureGatherCompare,
            core::BuiltinFn::kTextureNumLayers,
            core::BuiltinFn::kTextureNumLevels,
            core::BuiltinFn::kTextureNumSamples,
            core::BuiltinFn::kTextureSample,
            core::BuiltinFn::kTextureSampleBias,
            core::BuiltinFn::kTextureSampleCompare,
            core::BuiltinFn::kTextureSampleCompareLevel,
            core::BuiltinFn::kTextureSampleGrad,
            core::BuiltinFn::kTextureSampleLevel,
            core::BuiltinFn::kTextureSampleBaseClampToEdge,
            core::BuiltinFn::kTextureStore,
            core::BuiltinFn::kTextureLoad,
            core::BuiltinFn::kAtomicLoad,
            core::BuiltinFn::kAtomicStore,
            core::BuiltinFn::kAtomicAdd,
            core::BuiltinFn::kAtomicSub,
            core::BuiltinFn::kAtomicMax,
            core::BuiltinFn::kAtomicMin,
            core::BuiltinFn::kAtomicAnd,
            core::BuiltinFn::kAtomicOr,
            core::BuiltinFn::kAtomicXor,
            core::BuiltinFn::kAtomicExchange,
            core::BuiltinFn::kAtomicCompareExchangeWeak,
            core::BuiltinFn::kSubgroupBallot,
            core::BuiltinFn::kSubgroupBroadcast,
        });
    }

    Result<core::BuiltinValue> BuiltinValue() {
        return Enum({
            core::BuiltinValue::kPointSize,
            core::BuiltinValue::kFragDepth,
            core::BuiltinValue::kFrontFacing,
            core::BuiltinValue::kGlobalInvocationId,
            core::BuiltinValue::kInstanceIndex,
            core::BuiltinValue::kLocalInvocationId,
            core::BuiltinValue::kLocalInvocationIndex,
            core::BuiltinValue::kNumWorkgroups,
            core::BuiltinValue::kPosition,
            core::BuiltinValue::kSampleIndex,
            core::BuiltinValue::kSampleMask,
            core::BuiltinValue::kSubgroupInvocationId,
            core::BuiltinValue::kSubgroupSize,
            core::BuiltinValue::kVertexIndex,
            core::BuiltinValue::kWorkgroupId,
        });
    }

    Result<core::Interpolation> Interpolation() {
        auto type = Enum({
            core::InterpolationType::kFlat,
            core::InterpolationType::kLinear,
            core::InterpolationType::kPerspective,
        });
        if (!type) {
            return type.Failure();
        }
        auto sampling = Enum({
            core::InterpolationSampling::kCenter,
            core::InterpolationSampling::kCentroid,
            core::InterpolationSampling::kSample,
        });
        if (!sampling) {
            return sampling.Failure();
        }
        return core::Interpolation{type.Get(), sampling.Get()};
    }

    Result<core::AddressSpace> AddressSpace() {
        return Enum({
            core::AddressSpace::kFunction,
            core::AddressSpace::kHandle,
            core::AddressSpace::kPixelLocal,
            core::AddressSpace::kPrivate,
            core::AddressSpace::kPushConstant,
            core::AddressSpace::kStorage,
            core::AddressSpace::kUniform,
            core::AddressSpace::kWorkgroup,
        });
    }

    Result<core::Access> Access() {
        return Enum({
            core::Access::kRead,
            core::Access::kReadWrite,
            core::Access::kWrite,
        });
    }

    Result<core::ir::UnaryOp> UnaryOp() {
        return Enum({
            core::ir::UnaryOp::kComplement,
            core::ir::UnaryOp::kNegation,
        });
    }

    Result<core::ir::BinaryOp> BinaryOp() {
        return Enum({
            core::ir::BinaryOp::kAdd,
            core::ir::BinaryOp::kSubtract,
            core::ir::BinaryOp::kMultiply,
            core::ir::BinaryOp::kDivide,
            core::ir::BinaryOp::kModulo,
            core::ir::BinaryOp::kAnd,
            core::ir::BinaryOp::kOr,
            core::ir::BinaryOp::kXor,
            core::ir::BinaryOp::kEqual,
            core::ir::BinaryOp::kNotEqual,
            core::ir::BinaryOp::kLessThan,
            core::ir::BinaryOp::kGreaterThan,
            core::ir::BinaryOp::kLessThanEqual,
            core::ir::BinaryOp::kGreaterThanEqual,
            core::ir::BinaryOp::kShiftLeft,
            core::ir::BinaryOp::kShiftRight,
        });
    }

    std::array<uint32_t, 3> WorkgroupSize() {
        return {r.Int<uint8_t>(), r.Int<uint8_t>(), r.Int<uint8_t>()};
    }

    std::string IdentString(size_t max_len) {
        std::string str = r.String(max_len);
        size_t offset = 0;
        {
            auto [code_point, n] = utf8::Decode(str, 0);
            if (n == 0 || !code_point.IsXIDStart()) {
                return "";
            }
            offset = n;
        }
        while (offset < str.length()) {
            auto [code_point, n] = utf8::Decode(str, offset);
            if (n == 0 || !code_point.IsXIDContinue()) {
                break;
            }
            offset += n;
        }

        return str.substr(0, offset);
    }
};

}  // namespace
}  // namespace tint

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const int kRejectInput = -1;

    if (size > sizeof(tint::fuzz::ir::IRFuzzerID)) {
        tint::bytes::Reader reader{tint::Slice{data, size}};
        auto fuzzer_id = reader.Int<tint::fuzz::ir::IRFuzzerID>();
        tint::Builder builder{reader};
        auto mod = builder.Build();
        if (!mod) {
            return kRejectInput;
        }
        if (!tint::core::ir::Validate(mod.Get())) {
            return 0;
        }
        tint::fuzz::ir::Run(fuzzer_id, mod.Get());
    }
    return 0;
}
