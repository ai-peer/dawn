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
#include "src/tint/type/bool.h"
#include "src/tint/type/f16.h"
#include "src/tint/type/f32.h"
#include "src/tint/type/i32.h"
#include "src/tint/type/matrix.h"
#include "src/tint/type/struct.h"
#include "src/tint/type/type.h"
#include "src/tint/type/u32.h"
#include "src/tint/type/vector.h"

namespace wgpu::binding {

namespace {

interop::Interface<interop::WGSLType> createType(Napi::Env env,
                                                 const tint::Program* program,
                                                 const tint::type::Type* type) {
    return tint::Switch(
        type,  //
        [&](const tint::type::I32*) {
            return interop::WGSLType::Create<WGSLBasicType>(env, interop::WGSLKind::kI32, program,
                                                            type);
        },
        [&](const tint::type::U32*) {
            return interop::WGSLType::Create<WGSLBasicType>(env, interop::WGSLKind::kU32, program,
                                                            type);
        },
        [&](const tint::type::F32*) {
            return interop::WGSLType::Create<WGSLBasicType>(env, interop::WGSLKind::kF32, program,
                                                            type);
        },
        [&](const tint::type::Bool*) {
            return interop::WGSLType::Create<WGSLBasicType>(env, interop::WGSLKind::kBool, program,
                                                            type);
        },
        [&](const tint::type::Vector* ty) -> interop::Interface<interop::WGSLType> {
            return interop::WGSLVectorType::Create<WGSLVectorType>(env, program, ty);
        },
        [&](const tint::type::Matrix* ty) -> interop::Interface<interop::WGSLType> {
            return interop::WGSLMatrixType::Create<WGSLMatrixType>(env, program, ty);
        },
        [&](const tint::type::Array* ty) -> interop::Interface<interop::WGSLType> {
            return interop::WGSLArrayType::Create<WGSLArrayType>(env, program, ty);
        },
        [&](const tint::type::Struct* ty) -> interop::Interface<interop::WGSLType> {
            return interop::WGSLStructType::Create<WGSLStructType>(env, program, ty);
        },
        [&](tint::Default) {
            UNREACHABLE("Unhandled type", type->TypeInfo().name);
            return interop::Interface<interop::WGSLType>{};
        });
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WGSLBasicType
////////////////////////////////////////////////////////////////////////////////
WGSLBasicType::WGSLBasicType(interop::WGSLKind kind,
                             const tint::Program* program,
                             const tint::type::Type* type)
    : kind_(kind), program_(program), type_(type) {}

WGSLBasicType::~WGSLBasicType() = default;

interop::WGSLKind WGSLBasicType::getKind(Napi::Env) {
    return kind_;
}

interop::GPUSize64 WGSLBasicType::getSize(Napi::Env) {
    return type_->Size();
}

interop::GPUSize64 WGSLBasicType::getAlign(Napi::Env) {
    return type_->Align();
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

interop::Interface<interop::WGSLType> WGSLVectorType::getElementType(Napi::Env env) {
    return createType(env, program_, type_->type());
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

interop::Interface<interop::WGSLType> WGSLMatrixType::getElementType(Napi::Env env) {
    return createType(env, program_, type_->type());
}

interop::Interface<interop::WGSLType> WGSLMatrixType::getColumnType(Napi::Env env) {
    return createType(env, program_, type_->ColumnType());
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

interop::Interface<interop::WGSLType> WGSLArrayType::getElementType(Napi::Env env) {
    return createType(env, program_, type_->ElemType());
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

interop::Interface<interop::WGSLType> WGSLStructMember::getType(Napi::Env env) {
    return createType(env, program_, member_->Type());
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
// WGSLBindGroupEntry
////////////////////////////////////////////////////////////////////////////////
WGSLBindGroupEntry::WGSLBindGroupEntry(const tint::Program* program,
                                       const tint::sem::GlobalVariable* global)
    : program_(program), global_(global) {}

WGSLBindGroupEntry::~WGSLBindGroupEntry() = default;

interop::GPUIndex32 WGSLBindGroupEntry::getGroup(Napi::Env) {
    return global_->BindingPoint()->group;
}

interop::GPUIndex32 WGSLBindGroupEntry::getBinding(Napi::Env) {
    return global_->BindingPoint()->binding;
}

std::string WGSLBindGroupEntry::getName(Napi::Env) {
    return program_->Symbols().NameFor(global_->Declaration()->name->symbol);
}

interop::Interface<interop::WGSLType> WGSLBindGroupEntry::getType(Napi::Env env) {
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

std::vector<interop::Interface<interop::WGSLBindGroupEntry>> WGSLBindGroup::values(Napi::Env env) {
    std::vector<interop::Interface<interop::WGSLBindGroupEntry>> out;
    std::unordered_set<uint32_t> set;
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group_) {
                if (set.emplace(bp->binding).second) {
                    out.push_back(interop::WGSLBindGroupEntry::Create<WGSLBindGroupEntry>(
                        env, program_, global));
                }
            }
        }
    }
    return out;
}

interop::Interface<interop::WGSLBindGroupEntry> WGSLBindGroup::get(Napi::Env env,
                                                                   interop::GPUIndex32 binding) {
    for (auto global : fn_->TransitivelyReferencedGlobals()) {
        if (auto bp = global->BindingPoint()) {
            if (bp->group == group_ && bp->binding == binding) {
                return interop::WGSLBindGroupEntry::Create<WGSLBindGroupEntry>(env, program_,
                                                                               global);
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
