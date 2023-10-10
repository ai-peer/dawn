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
            if (!TaggedDispatch([&] { return AddToRootBlock(); }, [&] { return Function(); })) {
                return Failure{};
            }
        }
        return std::move(ir);
    }

  private:
    using ValueID = uint16_t;

    bytes::Reader r;
    core::ir::Module ir{};
    core::ir::Builder b{ir};
    Hashmap<uint8_t, core::ir::Function*, 8> fns_{};
    Hashmap<ValueID, core::ir::Value*, 256> values_{};
    Hashmap<uint8_t, const core::type::Pointer*, 8> ptrs_{};
    Hashmap<uint8_t, const core::type::Array*, 8> arrays_{};
    Hashmap<uint8_t, const core::type::Struct*, 8> structs_{};

    template <typename... FNS>
    [[nodiscard]] bool TaggedDispatch(FNS&&... fns) {
        Vector<std::function<bool()>, sizeof...(fns)> funcs{fns...};
        while (!r.IsEOF()) {
            uint32_t i = r.Int<uint32_t>();
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
        return fns_.GetOrCreate(r.Int<uint8_t>(), [&]() -> core::ir::Function* {
            auto name = IdentString(3);
            if (!name) {
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
            auto* fn = b.Function(name.Get(), ret_ty, stage, workgroup_size);
            fn->SetParams(std::move(params));
            if (!PopulateBlock(fn->Block())) {
                return nullptr;
            }
            return fn;
        });
    }

    core::ir::FunctionParam* Parameter() {
        auto name = IdentString(3);
        if (!name) {
            return nullptr;
        }
        auto ty = Type();
        if (!ty) {
            return nullptr;
        }
        return b.FunctionParam(name.Get(), ty);
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
            case 0: {
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
                inst = b.Unary(op.Get(), ty, val);
                break;
            }
            case 1: {
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
                inst = b.Binary(op.Get(), ty, lhs, rhs);
                break;
            }
            case 10: {
                // auto* ty = Type();
                // if (!ty) {
                //     return nullptr;
                // }
                // inst = b.Binary(op, ty, lhs, rhs);
                break;
            }
            default:
                return nullptr;
        }

        if (!inst) {
            return nullptr;
        }

        for (auto* res : inst->Results()) {
            auto id = r.Int<ValueID>();
            if (!values_.Add(id, res)) {
                return nullptr;
            }
        }

        return inst;
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
        return ptrs_.GetOrCreate(r.Int<uint8_t>(), [&]() -> const core::type::Pointer* {
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
        return arrays_.GetOrCreate(r.Int<uint8_t>(), [&]() -> const core::type::Array* {
            auto ty = Type();
            if (!ty || ty->Size() == 0) {
                return nullptr;
            }
            uint32_t n = r.Int<uint32_t>();
            return b.ir.Types().array(ty, n);
        });
    }

    const core::type::Struct* Struct() {
        return structs_.GetOrCreate(r.Int<uint8_t>(), [&]() -> const core::type::Struct* {
            auto name = IdentString(3);
            if (!name) {
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
            return b.ir.Types().Struct(b.ir.symbols.Register(name.Get()), members);
        });
    }

    Result<core::type::Manager::StructMemberDesc> StructMemberDesc() {
        auto name = IdentString(3);
        if (!name) {
            return name.Failure();
        }
        core::type::Manager::StructMemberDesc out;
        out.name = b.ir.symbols.Register(name.Get());
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

    Result<std::string> IdentString(size_t len) {
        std::string str = r.String(len);

        auto* utf8 = reinterpret_cast<const uint8_t*>(str.data());
        len = str.size();

        {
            auto [code_point, n] = utf8::Decode(utf8, len);
            if (n == 0 || !code_point.IsXIDStart()) {
                return Failure{};
            }
            len -= n;
        }
        while (len > 0) {
            auto [code_point, n] = utf8::Decode(utf8, len);
            if (n == 0 || !code_point.IsXIDContinue()) {
                return Failure{};
            }
            len -= n;
        }

        return str;
    }
};

}  // namespace
}  // namespace tint

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > sizeof(tint::fuzz::ir::IRFuzzerID)) {
        tint::bytes::Reader reader{tint::Slice{data, size}};
        auto fuzzer_id = reader.Int<tint::fuzz::ir::IRFuzzerID>();
        tint::Builder builder{reader};
        if (auto mod = builder.Build()) {
            tint::fuzz::ir::Run(fuzzer_id, mod.Get());
        }
    }
    return 0;
}
