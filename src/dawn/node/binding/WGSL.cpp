// Copyright 2021 The Dawn Authors
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

#include "src/dawn/node/binding/WGSL.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "src/dawn/node/utils/Debug.h"

#include "src/tint/ast/identifier.h"
#include "src/tint/ast/module.h"
#include "src/tint/program.h"
#include "src/tint/sem/function.h"
#include "src/tint/switch.h"
#include "src/tint/type/array.h"
#include "src/tint/type/atomic.h"
#include "src/tint/type/bool.h"
#include "src/tint/type/depth_multisampled_texture.h"
#include "src/tint/type/depth_texture.h"
#include "src/tint/type/f16.h"
#include "src/tint/type/f32.h"
#include "src/tint/type/i32.h"
#include "src/tint/type/matrix.h"
#include "src/tint/type/multisampled_texture.h"
#include "src/tint/type/sampled_texture.h"
#include "src/tint/type/struct.h"
#include "src/tint/type/type.h"
#include "src/tint/type/u32.h"
#include "src/tint/type/vector.h"

namespace wgpu::binding {

namespace {

interop::Interface<interop::WGSLTypeBase> createType(Napi::Env env,
                                                     const tint::Program* program,
                                                     const tint::type::Type* type) {
    return tint::Switch(
        type,  //
        [&](const tint::type::I32* ty) {
            return interop::WGSLSizedType::Create<WGSLScalarType>(env, interop::WGSLKind::kI32,
                                                                  ty->Size(), ty->Align());
        },
        [&](const tint::type::U32* ty) {
            return interop::WGSLSizedType::Create<WGSLScalarType>(env, interop::WGSLKind::kU32,
                                                                  ty->Size(), ty->Align());
        },
        [&](const tint::type::F32* ty) {
            return interop::WGSLSizedType::Create<WGSLScalarType>(env, interop::WGSLKind::kF32,
                                                                  ty->Size(), ty->Align());
        },
        [&](const tint::type::Bool* ty) {
            return interop::WGSLSizedType::Create<WGSLScalarType>(env, interop::WGSLKind::kBool,
                                                                  ty->Size(), ty->Align());
        },
        [&](const tint::type::Atomic* ty) {
            return interop::WGSLAtomicType::Create<WGSLAtomicType>(env, program, ty);
        },
        [&](const tint::type::Vector* ty) {
            return interop::WGSLVectorType::Create<WGSLVectorType>(env, program, ty);
        },
        [&](const tint::type::Matrix* ty) {
            return interop::WGSLMatrixType::Create<WGSLMatrixType>(env, program, ty);
        },
        [&](const tint::type::Array* ty) {
            return interop::WGSLArrayType::Create<WGSLArrayType>(env, program, ty);
        },
        [&](const tint::type::Struct* ty) {
            return interop::WGSLStructType::Create<WGSLStructType>(env, program, ty);
        },
        [&](const tint::type::Sampler* ty) {
            return interop::WGSLTypeBase::Create<WGSLUnsizedType>(
                env, ty->IsComparison() ? interop::WGSLKind::kSamplerComparison
                                        : interop::WGSLKind::kSampler);
        },
        [&](const tint::type::SampledTexture* ty) {
            return interop::WGSLSampledTextureType::Create<WGSLSampledTextureType>(env, program,
                                                                                   ty);
        },
        [&](const tint::type::DepthTexture* ty) {
            return interop::WGSLDepthTextureType::Create<WGSLDepthTextureType>(env, program, ty);
        },
        [&](const tint::type::MultisampledTexture* ty) {
            return interop::WGSLMultisampledTextureType::Create<WGSLMultisampledTextureType>(
                env, program, ty);
        },
        [&](const tint::type::DepthMultisampledTexture* ty) {
            return interop::WGSLTypeBase::Create<WGSLUnsizedType>(
                env, interop::WGSLKind::kDepthMultisampledTexture);
        },
        [&](tint::Default) {
            UNREACHABLE("Unhandled type", type->TypeInfo().name);
            return interop::Interface<interop::WGSLTypeBase>{};
        });
}

interop::Interface<interop::WGSLSizedType> createSizedType(Napi::Env env,
                                                           const tint::Program* program,
                                                           const tint::type::Type* type) {
    auto ty = createType(env, program, type);
    return interop::Interface<interop::WGSLSizedType>(Napi::Object(ty));
}

interop::GPUTextureViewDimension convert(tint::type::TextureDimension dim) {
    switch (dim) {
        case tint::type::TextureDimension::k1d:
            return interop::GPUTextureViewDimension::k1D;
        case tint::type::TextureDimension::k2d:
            return interop::GPUTextureViewDimension::k2D;
        case tint::type::TextureDimension::k2dArray:
            return interop::GPUTextureViewDimension::k2DArray;
        case tint::type::TextureDimension::kCube:
            return interop::GPUTextureViewDimension::kCube;
        case tint::type::TextureDimension::kCubeArray:
            return interop::GPUTextureViewDimension::kCubeArray;
        case tint::type::TextureDimension::k3d:
            return interop::GPUTextureViewDimension::k3D;
        default:
            UNREACHABLE();
    }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WGSLUnsizedType
////////////////////////////////////////////////////////////////////////////////
WGSLUnsizedType::WGSLUnsizedType(interop::WGSLKind kind) : kind_(kind) {}

WGSLUnsizedType::~WGSLUnsizedType() = default;

interop::WGSLKind WGSLUnsizedType::getKind(Napi::Env) {
    return kind_;
}

////////////////////////////////////////////////////////////////////////////////
// WGSLScalarType
////////////////////////////////////////////////////////////////////////////////
WGSLScalarType::WGSLScalarType(interop::WGSLKind kind, uint64_t size, uint64_t align)
    : kind_(kind), size_(size), align_(align) {}

WGSLScalarType::~WGSLScalarType() = default;

interop::WGSLKind WGSLScalarType::getKind(Napi::Env) {
    return kind_;
}

interop::GPUSize64 WGSLScalarType::getSize(Napi::Env) {
    return size_;
}

interop::GPUSize64 WGSLScalarType::getAlign(Napi::Env) {
    return align_;
}

////////////////////////////////////////////////////////////////////////////////
// WGSLAtomicType
////////////////////////////////////////////////////////////////////////////////
WGSLAtomicType::WGSLAtomicType(const tint::Program* program, const tint::type::Atomic* type)
    : program_(program), type_(type) {}

WGSLAtomicType::~WGSLAtomicType() = default;

interop::WGSLKind WGSLAtomicType::getKind(Napi::Env) {
    return interop::WGSLKind::kAtomic;
}

interop::GPUSize64 WGSLAtomicType::getSize(Napi::Env) {
    return type_->Size();
}

interop::GPUSize64 WGSLAtomicType::getAlign(Napi::Env) {
    return type_->Align();
}

interop::Interface<interop::WGSLSizedType> WGSLAtomicType::getElementType(Napi::Env env) {
    return createSizedType(env, program_, type_->Type());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLVectorType
////////////////////////////////////////////////////////////////////////////////
WGSLVectorType::WGSLVectorType(const tint::Program* program, const tint::type::Vector* type)
    : program_(program), type_(type) {}

WGSLVectorType::~WGSLVectorType() = default;

interop::WGSLKind WGSLVectorType::getKind(Napi::Env) {
    return interop::WGSLKind::kVector;
}

interop::GPUSize64 WGSLVectorType::getSize(Napi::Env) {
    return type_->Size();
}

interop::GPUSize64 WGSLVectorType::getAlign(Napi::Env) {
    return type_->Align();
}

interop::GPUSize64 WGSLVectorType::getElementCount(Napi::Env) {
    return type_->Width();
}

interop::Interface<interop::WGSLSizedType> WGSLVectorType::getElementType(Napi::Env env) {
    return createSizedType(env, program_, type_->type());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLMatrixType
////////////////////////////////////////////////////////////////////////////////
WGSLMatrixType::WGSLMatrixType(const tint::Program* program, const tint::type::Matrix* type)
    : program_(program), type_(type) {}

WGSLMatrixType::~WGSLMatrixType() = default;

interop::WGSLKind WGSLMatrixType::getKind(Napi::Env) {
    return interop::WGSLKind::kMatrix;
}

interop::GPUSize64 WGSLMatrixType::getSize(Napi::Env) {
    return type_->Size();
}

interop::GPUSize64 WGSLMatrixType::getAlign(Napi::Env) {
    return type_->Align();
}

interop::GPUSize64 WGSLMatrixType::getColumnCount(Napi::Env) {
    return type_->columns();
}

interop::GPUSize64 WGSLMatrixType::getRowCount(Napi::Env) {
    return type_->rows();
}

interop::Interface<interop::WGSLSizedType> WGSLMatrixType::getElementType(Napi::Env env) {
    return createSizedType(env, program_, type_->type());
}

interop::Interface<interop::WGSLSizedType> WGSLMatrixType::getColumnType(Napi::Env env) {
    return createSizedType(env, program_, type_->ColumnType());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLArrayType
////////////////////////////////////////////////////////////////////////////////
WGSLArrayType::WGSLArrayType(const tint::Program* program, const tint::type::Array* type)
    : program_(program), type_(type) {}

WGSLArrayType::~WGSLArrayType() = default;

interop::WGSLKind WGSLArrayType::getKind(Napi::Env) {
    return interop::WGSLKind::kArray;
}

interop::GPUSize64 WGSLArrayType::getSize(Napi::Env) {
    return type_->Size();
}

interop::GPUSize64 WGSLArrayType::getAlign(Napi::Env) {
    return type_->Align();
}

interop::WGSLArrayCount WGSLArrayType::getElementCount(Napi::Env) {
    if (auto* count = type_->Count()->As<tint::type::ConstantArrayCount>()) {
        return count->value;
    }
    return interop::WGSLRuntimeSizedArrayCount::kRuntimeSized;
}

interop::Interface<interop::WGSLSizedType> WGSLArrayType::getElementType(Napi::Env env) {
    return createSizedType(env, program_, type_->ElemType());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLStructMember
////////////////////////////////////////////////////////////////////////////////
WGSLStructMember::WGSLStructMember(const tint::Program* program,
                                   const tint::type::StructMember* member)
    : program_(program), member_(member) {}

WGSLStructMember::~WGSLStructMember() = default;

std::string WGSLStructMember::getName(Napi::Env) {
    return program_->Symbols().NameFor(member_->Name());
}

interop::Interface<interop::WGSLSizedType> WGSLStructMember::getType(Napi::Env env) {
    return createSizedType(env, program_, member_->Type());
}

interop::GPUIndex32 WGSLStructMember::getIndex(Napi::Env) {
    return member_->Index();
}

interop::GPUSize64 WGSLStructMember::getOffset(Napi::Env) {
    return member_->Offset();
}

interop::GPUSize64 WGSLStructMember::getSize(Napi::Env) {
    return member_->Size();
}

interop::GPUSize64 WGSLStructMember::getAlign(Napi::Env) {
    return member_->Align();
}

////////////////////////////////////////////////////////////////////////////////
// WGSLStructType
////////////////////////////////////////////////////////////////////////////////
WGSLStructType::WGSLStructType(const tint::Program* program, const tint::type::Struct* type)
    : program_(program), type_(type) {}

WGSLStructType::~WGSLStructType() = default;

interop::WGSLKind WGSLStructType::getKind(Napi::Env) {
    return interop::WGSLKind::kStruct;
}

interop::GPUSize64 WGSLStructType::getSize(Napi::Env) {
    return type_->Size();
}

interop::GPUSize64 WGSLStructType::getAlign(Napi::Env) {
    return type_->Align();
}

std::string WGSLStructType::getName(Napi::Env) {
    return program_->Symbols().NameFor(type_->Name());
}

interop::FrozenArray<interop::Interface<interop::WGSLStructMember>> WGSLStructType::getMembers(
    Napi::Env env) {
    interop::FrozenArray<interop::Interface<interop::WGSLStructMember>> out;
    for (auto* member : type_->Members()) {
        out.push_back(interop::WGSLStructMember::Create<WGSLStructMember>(env, program_, member));
    }
    return out;
}

////////////////////////////////////////////////////////////////////////////////
// WGSLSampledTextureType
////////////////////////////////////////////////////////////////////////////////
WGSLSampledTextureType::WGSLSampledTextureType(const tint::Program* program,
                                               const tint::type::SampledTexture* type)
    : program_(program), type_(type) {}

WGSLSampledTextureType::~WGSLSampledTextureType() = default;

interop::WGSLKind WGSLSampledTextureType::getKind(Napi::Env) {
    return interop::WGSLKind::kSampledTexture;
}

interop::GPUTextureViewDimension WGSLSampledTextureType::getDimensions(Napi::Env) {
    return convert(type_->dim());
}

interop::Interface<interop::WGSLSizedType> WGSLSampledTextureType::getSampledType(Napi::Env env) {
    return createSizedType(env, program_, type_->type());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLMultisampledTextureType
////////////////////////////////////////////////////////////////////////////////
WGSLMultisampledTextureType::WGSLMultisampledTextureType(
    const tint::Program* program,
    const tint::type::MultisampledTexture* type)
    : program_(program), type_(type) {}

WGSLMultisampledTextureType::~WGSLMultisampledTextureType() = default;

interop::WGSLKind WGSLMultisampledTextureType::getKind(Napi::Env) {
    return interop::WGSLKind::kMultisampledTexture;
}

interop::Interface<interop::WGSLSizedType> WGSLMultisampledTextureType::getSampledType(
    Napi::Env env) {
    return createSizedType(env, program_, type_->type());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLDepthTextureType
////////////////////////////////////////////////////////////////////////////////
WGSLDepthTextureType::WGSLDepthTextureType(const tint::Program* program,
                                           const tint::type::DepthTexture* type)
    : program_(program), type_(type) {}

WGSLDepthTextureType::~WGSLDepthTextureType() = default;

interop::WGSLKind WGSLDepthTextureType::getKind(Napi::Env) {
    return interop::WGSLKind::kDepthTexture;
}

interop::GPUTextureViewDimension WGSLDepthTextureType::getDimensions(Napi::Env) {
    return convert(type_->dim());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLBindPoint
////////////////////////////////////////////////////////////////////////////////
WGSLBindPoint::WGSLBindPoint(const tint::Program* program, const tint::sem::GlobalVariable* global)
    : program_(program), global_(global) {}

WGSLBindPoint::~WGSLBindPoint() = default;

interop::GPUIndex32 WGSLBindPoint::getGroup(Napi::Env) {
    return global_->BindingPoint()->group;
}

interop::GPUIndex32 WGSLBindPoint::getBinding(Napi::Env) {
    return global_->BindingPoint()->binding;
}

std::string WGSLBindPoint::getName(Napi::Env) {
    return program_->Symbols().NameFor(global_->Declaration()->name->symbol);
}

interop::Interface<interop::WGSLTypeBase> WGSLBindPoint::getType(Napi::Env env) {
    return createType(env, program_, global_->Type()->UnwrapRef());
}

////////////////////////////////////////////////////////////////////////////////
// WGSLBindGroup
////////////////////////////////////////////////////////////////////////////////
WGSLBindGroup::WGSLBindGroup(const tint::Program* program,
                             const tint::sem::Function* fn,
                             uint32_t group)
    : program_(program), fn_(fn), group_(group) {}

WGSLBindGroup::~WGSLBindGroup() = default;

bool WGSLBindGroup::has(Napi::Env, interop::GPUIndex32 binding) {
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group_ && bp->binding == binding) {
                return true;
            }
        }
    }
    return false;
}

std::vector<interop::GPUIndex32> WGSLBindGroup::keys(Napi::Env) {
    std::vector<interop::GPUIndex32> out;
    std::unordered_set<uint32_t> set;
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group_) {
                if (set.emplace(bp->binding).second) {
                    out.push_back(bp->binding);
                }
            }
        }
    }
    return out;
}

std::vector<interop::Interface<interop::WGSLBindPoint>> WGSLBindGroup::values(Napi::Env env) {
    std::vector<interop::Interface<interop::WGSLBindPoint>> out;
    std::unordered_set<uint32_t> set;
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group_) {
                if (set.emplace(bp->binding).second) {
                    out.push_back(
                        interop::WGSLBindPoint::Create<WGSLBindPoint>(env, program_, global));
                }
            }
        }
    }
    return out;
}

interop::Interface<interop::WGSLBindPoint> WGSLBindGroup::get(Napi::Env env,
                                                              interop::GPUIndex32 binding) {
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group_ && bp->binding == binding) {
                return interop::WGSLBindPoint::Create<WGSLBindPoint>(env, program_, global);
            }
        }
    }
    return {};
}

interop::GPUIndex32 WGSLBindGroup::getGroup(Napi::Env) {
    return group_;
}

////////////////////////////////////////////////////////////////////////////////
// WGSLBindGroups
////////////////////////////////////////////////////////////////////////////////
WGSLBindGroups::WGSLBindGroups(const tint::Program* program, const tint::sem::Function* fn)
    : program_(program), fn_(fn) {}

WGSLBindGroups::~WGSLBindGroups() = default;

bool WGSLBindGroups::has(Napi::Env, interop::GPUIndex32 group) {
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group) {
                return true;
            }
        }
    }
    return false;
}

std::vector<interop::GPUIndex32> WGSLBindGroups::keys(Napi::Env) {
    std::vector<interop::GPUIndex32> out;
    std::unordered_set<uint32_t> set;
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (set.emplace(bp->group).second) {
                out.push_back(bp->group);
            }
        }
    }
    return out;
}

std::vector<interop::Interface<interop::WGSLBindGroup>> WGSLBindGroups::values(Napi::Env env) {
    std::vector<interop::Interface<interop::WGSLBindGroup>> out;
    std::unordered_set<uint32_t> set;
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (set.emplace(bp->group).second) {
                out.push_back(
                    interop::WGSLBindGroup::Create<WGSLBindGroup>(env, program_, fn_, bp->group));
            }
        }
    }
    return out;
}

interop::Interface<interop::WGSLBindGroup> WGSLBindGroups::get(Napi::Env env,
                                                               interop::GPUIndex32 group) {
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group) {
                return interop::WGSLBindGroup::Create<WGSLBindGroup>(env, program_, fn_, group);
            }
        }
    }
    return {};
}

////////////////////////////////////////////////////////////////////////////////
// WGSLEntryPoint
////////////////////////////////////////////////////////////////////////////////
WGSLEntryPoint::WGSLEntryPoint(const tint::Program* program, const tint::sem::Function* fn)
    : program_(program), fn_(fn) {}

WGSLEntryPoint::~WGSLEntryPoint() = default;

interop::WGSLShaderStage WGSLEntryPoint::getStage(Napi::Env) {
    switch (fn_->Declaration()->PipelineStage()) {
        case tint::ast::PipelineStage::kCompute:
            return interop::WGSLShaderStage::kCompute;
        case tint::ast::PipelineStage::kFragment:
            return interop::WGSLShaderStage::kFragment;
        case tint::ast::PipelineStage::kVertex:
            return interop::WGSLShaderStage::kVertex;
        case tint::ast::PipelineStage::kNone:
            break;
    }
    UNREACHABLE();
}

interop::Interface<interop::WGSLBindGroups> WGSLEntryPoint::getBindgroups(Napi::Env env) {
    return interop::WGSLBindGroups::Create<WGSLBindGroups>(env, program_, fn_);
}

std::string WGSLEntryPoint::getName(Napi::Env) {
    return program_->Symbols().NameFor(fn_->Declaration()->name->symbol);
}

////////////////////////////////////////////////////////////////////////////////
// WGSLEntryPoints
////////////////////////////////////////////////////////////////////////////////
WGSLEntryPoints::WGSLEntryPoints(const tint::Program* program) : program_(program) {}

WGSLEntryPoints::~WGSLEntryPoints() = default;

bool WGSLEntryPoints::has(Napi::Env, std::string name) {
    std::vector<std::string> out;
    for (auto* fn_ : program_->AST().Functions()) {
        if (fn_->IsEntryPoint()) {
            if (program_->Symbols().NameFor(fn_->name->symbol) == name) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> WGSLEntryPoints::keys(Napi::Env) {
    std::vector<std::string> out;
    for (auto* fn_ : program_->AST().Functions()) {
        if (fn_->IsEntryPoint()) {
            out.push_back(program_->Symbols().NameFor(fn_->name->symbol));
        }
    }
    return out;
}

std::vector<interop::Interface<interop::WGSLEntryPoint>> WGSLEntryPoints::values(Napi::Env env) {
    std::vector<interop::Interface<interop::WGSLEntryPoint>> out;
    for (auto* fn_ : program_->AST().Functions()) {
        if (fn_->IsEntryPoint()) {
            auto* sem = program_->Sem().Get(fn_);
            out.push_back(interop::WGSLEntryPoint::Create<WGSLEntryPoint>(env, program_, sem));
        }
    }
    return out;
}

interop::Interface<interop::WGSLEntryPoint> WGSLEntryPoints::get(Napi::Env env, std::string name) {
    for (auto* fn_ : program_->AST().Functions()) {
        if (fn_->IsEntryPoint()) {
            if (program_->Symbols().NameFor(fn_->name->symbol) == name) {
                auto* sem = program_->Sem().Get(fn_);
                return interop::WGSLEntryPoint::Create<WGSLEntryPoint>(env, program_, sem);
            }
        }
    }
    return {};
}

}  // namespace wgpu::binding
