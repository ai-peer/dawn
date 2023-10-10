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

namespace {

class Builder {
  public:
    tint::bytes::Reader r;
    tint::core::ir::Builder b;

    bool Build() {
        while (r.IsEOF()) {
            if (!TaggedDispatch([&] { return AddToRootBlock(); },
                                [&] { return Function() != nullptr; })) {
                return false;
            }
        }
        return true;
    }

  private:
    tint::Hashmap<uint8_t, tint::core::ir::Function*, 8> fns_;
    tint::Hashmap<uint8_t, tint::core::type::Pointer*, 8> ptrs_;
    tint::Hashmap<uint8_t, tint::core::type::Array*, 8> arrays_;
    tint::Hashmap<uint8_t, tint::core::type::Struct*, 8> structs_;
    tint::Hashmap<uint16_t, tint::core::ir::Value*, 256> values_;

    template <typename... FNS>
    [[nodiscard]] bool TaggedDispatch(FNS&&... fns) {
        tint::Vector<std::function<void()>> funcs{fns...};
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
    [[nodiscard]] bool Enum(ENUM& out, std::initializer_list<ENUM> values) {
        auto i = r.Int<uint8_t>();
        if (i >= values.size) {
            return false;
        }
        out = values.begin()[i];
        return true;
    }

    [[nodiscard]] bool AddToRootBlock() {
        auto id = r.Int<uint16_t>();
        auto* ptr = Ptr();
        if (!ptr) {
            return false;
        }
        auto var = b.ir.root_block->Append(b.Var(ptr));
        return values_.Add(id, var->Result());
    }

    [[nodiscard]] tint::core::ir::Function* Function() {
        return fns_.GetOrCreate(r.Int<uint8_t>(), [&] -> tint::core::ir::Function* {
            auto stage = tint::core::ir::Function::PipelineStage::kUndefined;
            std::optional<std::array<uint32_t, 3UL>> workgroup_size;
            tint::Vector<tint::core::ir::FunctionParam*, 4> params;
            if (!TaggedDispatch([&] { return PipelineStage(stage); },
                                [&] {
                                    workgroup_size = {};
                                    return WorkgroupSize(workgroup_size.value());
                                },
                                [&] {
                                    if (auto* param = Parameter()) {
                                        params.Push(param);
                                        return true;
                                    }
                                    return false;
                                })) {
                return nullptr;
            }
            auto* fn = b.Function(r.String(3), Type(), stage, workgroup_size);
            fn->SetParams(std::move(params));
            PopulateBlock(fn->Block());
            return fn;
        });
    }

    [[nodiscard]] tint::core::ir::FunctionParam* Parameter() {
        auto name = r.String(3);
        auto* ty = Type();
        return b.FunctionParam(name, ty);
    }

    [[nodiscard]] bool PopulateBlock(tint::core::ir::Block* block) {
        return TaggedDispatch(
            [&] {
                if (auto* inst = Instruction()) {
                    block->Append(inst);
                    return true;
                }
                return false;
            },
            [&] {
                if (auto* inst = Instruction()) {
                    block->Prepend(inst);
                    return true;
                }
                return false;
            });
    }

    [[nodiscard]] tint::core::ir::Instruction* Instruction() {
        switch (r.Int<uint8_t>()) {
            case 0: {
                tint::core::ir::Binary::Kind op;
                auto* ty = Type();
                auto* lhs = Value();
                auto* rhs = Value();
                if (!ty || !lhs || !rhs || !BinaryOp(op)) {
                    return false;
                }
                b.Binary(op, ty, lhs, rhs);
            }
            default:
                return nullptr;
        }
    }

    [[nodiscard]] const tint::core::type::Type* Type() {
        switch (r.Int<uint8_t>()) {
            case 0:
                return b.ir.Types().void_();
            case 1:
                return b.ir.Types().bool_();
            case 2:
                return b.ir.Types().i32();
            case 3:
                return b.ir.Types().u32();
            case 4:
                return b.ir.Types().f32();
            case 5:
                return b.ir.Types().f16();
            case 6:
                return b.ir.Types().vec2(Scalar());
            case 7:
                return b.ir.Types().vec3(Scalar());
            case 8:
                return b.ir.Types().vec4(Scalar());
            case 9:
                return b.ir.Types().mat2x2(Scalar());
            case 10:
                return b.ir.Types().mat2x3(Scalar());
            case 11:
                return b.ir.Types().mat2x4(Scalar());
            case 12:
                return b.ir.Types().mat3x2(Scalar());
            case 13:
                return b.ir.Types().mat3x3(Scalar());
            case 14:
                return b.ir.Types().mat3x4(Scalar());
            case 15:
                return b.ir.Types().mat4x2(Scalar());
            case 16:
                return b.ir.Types().mat4x3(Scalar());
            case 17:
                return b.ir.Types().mat4x4(Scalar());
            case 51:
                return Ptr();
            case 52:
                return Array();
            case 53:
                return Struct();
        }
        return nullptr;
    }

    [[nodiscard]] const tint::core::type::Scalar* Scalar() {
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

    [[nodiscard]] const tint::core::type::Pointer* Ptr() {
        return ptrs_.GetOrCreate(r.Int<uint8_t>(), [&] -> const tint::core::type::Pointer* {
            tint::core::AddressSpace address_space;
            if (!AddressSpace(address_space)) {
                return nullptr;
            }
            tint::core::Access access;
            if (!Access(access)) {
                return nullptr;
            }
            auto* ty = Type();
            if (!ty) {
                return nullptr;
            }
            return b.ir.Types().ptr(address_space, ty, access);
        });
    }

    [[nodiscard]] const tint::core::type::Array* Array() {
        return arrays_.GetOrCreate(r.Int<uint8_t>(), [&] {
            auto* ty = Type();
            uint32_t n = r.Int<uint32_t>();
            return b.ir.Types().array(ty, n);
        });
    }

    [[nodiscard]] const tint::core::type::Struct* Struct() {
        return structs_.GetOrCreate(r.Int<uint8_t>(), [&] -> const tint::core::type::Struct* {
            auto name = b.ir.symbols.Register(r.String(3));
            tint::Vector<tint::core::type::Manager::StructMemberDesc, 4> members;
            members.Resize(r.Int<uint8_t>());
            for (auto& member : members) {
                if (!StructMemberDesc(member)) {
                    return nullptr;
                }
            }
            return b.ir.Types().Struct(name, members);
        });
    }

    [[nodiscard]] bool StructMemberDesc(tint::core::type::Manager::StructMemberDesc& out) {
        out.name = b.ir.symbols.Register(r.String(3));
        out.type = b.ir.Types().i32();
        return TaggedDispatch(
            [&] {
                out.type = Type();
                return out.type != nullptr;
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
                out.attributes.builtin = {};
                return BuiltinValue(out.attributes.builtin.value());
            },
            [&] {
                out.attributes.interpolation = {};
                return Interpolation(out.attributes.interpolation.value());
            },
            [&] {
                out.attributes.invariant = r.Bool();
                return true;
            });
    }

    [[nodiscard]] bool PipelineStage(tint::core::ir::Function::PipelineStage& out) {
        return Enum(out, {
                             tint::core::ir::Function::PipelineStage::kCompute,
                             tint::core::ir::Function::PipelineStage::kFragment,
                             tint::core::ir::Function::PipelineStage::kVertex,
                         });
    }

    [[nodiscard]] bool BuiltinValue(tint::core::BuiltinValue& out) {
        return Enum(out, {
                             tint::core::BuiltinValue::kPointSize,
                             tint::core::BuiltinValue::kFragDepth,
                             tint::core::BuiltinValue::kFrontFacing,
                             tint::core::BuiltinValue::kGlobalInvocationId,
                             tint::core::BuiltinValue::kInstanceIndex,
                             tint::core::BuiltinValue::kLocalInvocationId,
                             tint::core::BuiltinValue::kLocalInvocationIndex,
                             tint::core::BuiltinValue::kNumWorkgroups,
                             tint::core::BuiltinValue::kPosition,
                             tint::core::BuiltinValue::kSampleIndex,
                             tint::core::BuiltinValue::kSampleMask,
                             tint::core::BuiltinValue::kSubgroupInvocationId,
                             tint::core::BuiltinValue::kSubgroupSize,
                             tint::core::BuiltinValue::kVertexIndex,
                             tint::core::BuiltinValue::kWorkgroupId,
                         });
    }

    [[nodiscard]] bool Interpolation(tint::core::Interpolation& out) {
        if (!Enum(out.type, {
                                tint::core::InterpolationType::kFlat,
                                tint::core::InterpolationType::kLinear,
                                tint::core::InterpolationType::kPerspective,
                            })) {
            return false;
        }
        if (!Enum(out.sampling, {
                                    tint::core::InterpolationSampling::kCenter,
                                    tint::core::InterpolationSampling::kCentroid,
                                    tint::core::InterpolationSampling::kSample,
                                })) {
            return false;
        }
        return true;
    }

    [[nodiscard]] bool AddressSpace(tint::core::AddressSpace& out) {
        return Enum(out, {
                             tint::core::AddressSpace::kFunction,
                             tint::core::AddressSpace::kHandle,
                             tint::core::AddressSpace::kPixelLocal,
                             tint::core::AddressSpace::kPrivate,
                             tint::core::AddressSpace::kPushConstant,
                             tint::core::AddressSpace::kStorage,
                             tint::core::AddressSpace::kUniform,
                             tint::core::AddressSpace::kWorkgroup,
                         });
    }

    [[nodiscard]] bool Access(tint::core::Access& out) {
        return Enum(out, {
                             tint::core::Access::kRead,
                             tint::core::Access::kReadWrite,
                             tint::core::Access::kWrite,
                         });
    }

    [[nodiscard]] bool WorkgroupSize(std::array<uint32_t, 3>& out) {
        out = {r.Int<uint8_t>(), r.Int<uint8_t>(), r.Int<uint8_t>()};
        return true;
    }
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 2) {
        tint::bytes::Reader reader{tint::Slice{data, size}};
        Builder builder{reader};
        if (auto mod = builder.Build()) {
            tint::fuzz::ir::Run(mod.Get());
        }
    }
    return 0;
}
