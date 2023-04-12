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

// Forward declarations
interop::Interface<interop::WGSLTypeBase> createType(Napi::Env env,
                                                     const tint::Program* program,
                                                     const tint::type::Type* type);

interop::Interface<interop::WGSLSizedType> createSizedType(Napi::Env env,
                                                           const tint::Program* program,
                                                           const tint::type::Type* type);

template <typename T>
T align(T value, size_t alignment) {
    return alignment * ((value + (alignment - 1)) / alignment);
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

Napi::Value deserialize(Napi::Env env,
                        const tint::Program* program,
                        const tint::type::Type* type,
                        const uint8_t* ptr) {
    return tint::Switch<Napi::Value>(
        type,  //
        [&](const tint::type::I32* ty) {
            return Napi::Number::New(env, *reinterpret_cast<const int32_t*>(ptr));
        },
        [&](const tint::type::U32* ty) {
            return Napi::Number::New(env, *reinterpret_cast<const uint32_t*>(ptr));
        },
        [&](const tint::type::F32* ty) {
            return Napi::Number::New(env, *reinterpret_cast<const float*>(ptr));
        },
        [&](const tint::type::Atomic* ty) { return deserialize(env, program, ty->Type(), ptr); },
        [&](const tint::type::Vector* ty) {
            auto* elTy = ty->type();
            auto out = Napi::Array::New(env, ty->Width());
            for (uint32_t i = 0; i < ty->Width(); i++) {
                auto offset = elTy->Size() * i;
                out.Set(i, deserialize(env, program, elTy, ptr + offset));
            }
            return out;
        },
        [&](const tint::type::Matrix* ty) {
            auto out = Napi::Array::New(env, ty->columns());
            for (uint32_t i = 0; i < ty->columns(); i++) {
                auto offset = ty->ColumnStride() * i;
                out.Set(i, deserialize(env, program, ty->ColumnType(), ptr + offset));
            }
            return out;
        },
        [&](const tint::type::Array* ty) -> Napi::Value {
            if (auto count = ty->ConstantCount()) {
                auto* elTy = ty->ElemType();
                auto out = Napi::Array::New(env, *count);
                auto stride = align(elTy->Size(), elTy->Align());
                for (uint32_t i = 0; i < *count; i++) {
                    auto offset = stride * i;
                    out.Set(i, deserialize(env, program, elTy, ptr + offset));
                }
                return out;
            }
            return {};
        },
        [&](const tint::type::Struct* ty) {
            auto out = Napi::Object::New(env);
            for (auto* member : ty->Members()) {
                auto name = program->Symbols().NameFor(member->Name());
                auto value = deserialize(env, program, member->Type(), ptr + member->Offset());
                out.Set(name, value);
            }
            return out;
        },
        [&](tint::Default) {
            UNREACHABLE("Unhandled type", type->TypeInfo().name);
            return interop::Interface<interop::WGSLTypeBase>{};
        });
}

void serialize(Napi::Env env,
               const tint::Program* program,
               const tint::type::Type* type,
               uint8_t* ptr,
               Napi::Value value,
               interop::WGSLViewMissingValue missing) {
    tint::Switch(
        type,  //
        [&](const tint::type::I32* ty) {
            if (value.IsNull() || value.IsUndefined()) {
                if (missing == interop::WGSLViewMissingValue::kZero) {
                    *reinterpret_cast<int32_t*>(ptr) = 0;
                }
            } else {
                *reinterpret_cast<int32_t*>(ptr) = value.ToNumber().Int32Value();
            }
        },
        [&](const tint::type::U32* ty) {
            if (value.IsNull() || value.IsUndefined()) {
                if (missing == interop::WGSLViewMissingValue::kZero) {
                    *reinterpret_cast<int32_t*>(ptr) = 0;
                }
            } else {
                *reinterpret_cast<uint32_t*>(ptr) = value.ToNumber().Uint32Value();
            }
        },
        [&](const tint::type::F32* ty) {
            if (value.IsNull() || value.IsUndefined()) {
                if (missing == interop::WGSLViewMissingValue::kZero) {
                    *reinterpret_cast<int32_t*>(ptr) = 0;
                }
            } else {
                *reinterpret_cast<float*>(ptr) = value.ToNumber().FloatValue();
            }
        },
        [&](const tint::type::Atomic* ty) {
            serialize(env, program, ty->Type(), ptr, value, missing);
        },
        [&](const tint::type::Vector* ty) {
            auto* elTy = ty->type();
            if (value.IsArray()) {
                auto array = value.As<Napi::Array>();
                for (uint32_t i = 0; i < ty->Width(); i++) {
                    auto offset = elTy->Size() * i;
                    serialize(env, program, elTy, ptr + offset, array.Get(i), missing);
                }
            } else {
                UNIMPLEMENTED();
            }
        },
        [&](const tint::type::Matrix* ty) {
            if (value.IsArray()) {
                auto array = value.As<Napi::Array>();
                for (uint32_t i = 0; i < ty->columns(); i++) {
                    auto offset = ty->ColumnStride() * i;
                    serialize(env, program, ty->ColumnType(), ptr + offset, array.Get(i), missing);
                }
            }
        },
        [&](const tint::type::Array* ty) {
            if (value.IsArray()) {
                auto array = value.As<Napi::Array>();
                if (auto count = ty->ConstantCount()) {
                    auto* elTy = ty->ElemType();
                    auto stride = align(elTy->Size(), elTy->Align());
                    for (uint32_t i = 0; i < *count; i++) {
                        auto offset = stride * i;
                        serialize(env, program, elTy, ptr + offset, array.Get(i), missing);
                    }
                }
            }
        },
        [&](const tint::type::Struct* ty) {
            if (value.IsObject()) {
                auto object = value.As<Napi::Array>();
                for (auto* member : ty->Members()) {
                    auto name = program->Symbols().NameFor(member->Name());
                    auto val = object.Get(name);
                    serialize(env, program, member->Type(), ptr + member->Offset(), val, missing);
                }
            }
        },
        [&](tint::Default) { UNREACHABLE("Unhandled type", type->TypeInfo().name); });
}

class View : public interop::WGSLView {
  public:
    View(const tint::Program* program,
         const tint::type::Type* type,
         Napi::ArrayBuffer buffer,
         uint64_t offset,
         interop::WGSLViewMissingValue missingValue)
        : program_(program),
          type_(type),
          buffer_(Napi::Reference<Napi::ArrayBuffer>::New(buffer)),
          offset_(offset),
          missingValue_(missingValue) {}

    ~View() = default;

    interop::Interface<WGSLView> index(Napi::Env, interop::WGSLViewIndexOp op) override {
        UNIMPLEMENTED();
        return {};
    }

    interop::Any get(Napi::Env env) override {
        auto buffer = buffer_.Value();
        auto* base = reinterpret_cast<const uint8_t*>(buffer.Data());
        return deserialize(env, program_, type_, base + offset_);
    }

    void set(Napi::Env env, interop::Any value) override {
        auto buffer = buffer_.Value();
        auto* base = reinterpret_cast<uint8_t*>(buffer.Data());
        return serialize(env, program_, type_, base + offset_, value, missingValue_);
    }

  protected:
    tint::Program const* const program_;
    tint::type::Type const* const type_;
    Napi::Reference<Napi::ArrayBuffer> buffer_;
    uint64_t const offset_;
    interop::WGSLViewMissingValue missingValue_;
};

template <typename BASE, interop::WGSLKind KIND, typename TYPE = tint::type::Type>
class UnsizedType : public BASE {
  public:
    using Base = UnsizedType<BASE, KIND, TYPE>;

    UnsizedType(const tint::Program* program, const TYPE* type) : program_(program), type_(type) {}

    ~UnsizedType() = default;

    interop::WGSLKind getKind(Napi::Env) override { return KIND; }

  protected:
    tint::Program const* const program_;
    TYPE const* const type_;
};

template <typename BASE, interop::WGSLKind KIND, typename TYPE = tint::type::Type>
class SizedType : public BASE {
  public:
    using Base = SizedType<BASE, KIND, TYPE>;

    SizedType(const tint::Program* program, const TYPE* type) : program_(program), type_(type) {}

    ~SizedType() = default;

    interop::WGSLKind getKind(Napi::Env) override { return KIND; }

    interop::GPUSize64 getSize(Napi::Env) override { return type_->Size(); }

    interop::GPUSize64 getAlign(Napi::Env) override { return type_->Align(); }

    interop::Interface<interop::WGSLView> createView(
        Napi::Env env,
        interop::WGSLViewDescriptor descriptor) override {
        if (type_->Size() + descriptor.offset > descriptor.buffer.ByteLength()) {
            std::string err = "type size (" + std::to_string(type_->Size()) + ")";
            if (descriptor.offset > 0) {
                err += " + buffer offset (" + std::to_string(descriptor.offset) + ")";
            }
            err +=
                " exceeds buffer length (" + std::to_string(descriptor.buffer.ByteLength()) + ")";
            Napi::Error::New(env, err).ThrowAsJavaScriptException();
            return {};
        }
        return interop::WGSLView::Create<View>(env, program_, type_, descriptor.buffer,
                                               descriptor.offset, descriptor.missingValue);
    }

  protected:
    tint::Program const* const program_;
    TYPE const* const type_;
};

using I32Type = SizedType<interop::WGSLScalarType, interop::WGSLKind::kI32>;
using U32Type = SizedType<interop::WGSLScalarType, interop::WGSLKind::kU32>;
using F32Type = SizedType<interop::WGSLScalarType, interop::WGSLKind::kF32>;
using F16Type = SizedType<interop::WGSLScalarType, interop::WGSLKind::kF16>;
using BoolType = SizedType<interop::WGSLScalarType, interop::WGSLKind::kBool>;

class AtomicType final
    : public SizedType<interop::WGSLAtomicType, interop::WGSLKind::kAtomic, tint::type::Atomic> {
  public:
    AtomicType(const tint::Program* program, const tint::type::Atomic* type)
        : Base(program, type) {}

    interop::Interface<interop::WGSLSizedType> getElementType(Napi::Env env) override {
        return createSizedType(env, program_, type_->Type());
    }
};

class VectorType final
    : public SizedType<interop::WGSLVectorType, interop::WGSLKind::kVector, tint::type::Vector> {
  public:
    VectorType(const tint::Program* program, const tint::type::Vector* type)
        : Base(program, type) {}

    interop::GPUSize64 getElementCount(Napi::Env) override { return type_->Width(); }

    interop::Interface<interop::WGSLSizedType> getElementType(Napi::Env env) override {
        return createSizedType(env, program_, type_->type());
    }
};

class MatrixType final
    : public SizedType<interop::WGSLMatrixType, interop::WGSLKind::kMatrix, tint::type::Matrix> {
  public:
    MatrixType(const tint::Program* program, const tint::type::Matrix* type)
        : Base(program, type) {}

    interop::GPUSize64 getColumnCount(Napi::Env) override { return type_->columns(); }

    interop::GPUSize64 getRowCount(Napi::Env) override { return type_->rows(); }

    interop::Interface<interop::WGSLSizedType> getElementType(Napi::Env env) override {
        return createSizedType(env, program_, type_->type());
    }

    interop::Interface<interop::WGSLSizedType> getColumnType(Napi::Env env) override {
        return createSizedType(env, program_, type_->ColumnType());
    }
};

class ArrayType final
    : public SizedType<interop::WGSLArrayType, interop::WGSLKind::kArray, tint::type::Array> {
  public:
    ArrayType(const tint::Program* program, const tint::type::Array* type) : Base(program, type) {}

    interop::WGSLArrayCount getElementCount(Napi::Env) override {
        if (auto* count = type_->Count()->As<tint::type::ConstantArrayCount>()) {
            return count->value;
        }
        return interop::WGSLRuntimeSizedArrayCount::kRuntimeSized;
    }

    interop::Interface<interop::WGSLSizedType> getElementType(Napi::Env env) override {
        return createSizedType(env, program_, type_->ElemType());
    }
};

class StructType final
    : public SizedType<interop::WGSLStructType, interop::WGSLKind::kStruct, tint::type::Struct> {
    class Member final : public interop::WGSLStructMember {
      public:
        Member(const tint::Program* program, const tint::type::StructMember* member)
            : program_(program), member_(member) {}

        ~Member() = default;

        std::string getName(Napi::Env) override {
            return program_->Symbols().NameFor(member_->Name());
        }

        interop::Interface<interop::WGSLSizedType> getType(Napi::Env env) override {
            return createSizedType(env, program_, member_->Type());
        }

        interop::GPUIndex32 getIndex(Napi::Env) override { return member_->Index(); }

        interop::GPUSize64 getOffset(Napi::Env) override { return member_->Offset(); }

        interop::GPUSize64 getSize(Napi::Env) override { return member_->Size(); }

        interop::GPUSize64 getAlign(Napi::Env) override { return member_->Align(); }

      private:
        tint::Program const* const program_;
        tint::type::StructMember const* const member_;
    };

  public:
    StructType(const tint::Program* program, const tint::type::Struct* type)
        : Base(program, type) {}

    std::string getName(Napi::Env) override { return program_->Symbols().NameFor(type_->Name()); }

    interop::FrozenArray<interop::Interface<interop::WGSLStructMember>> getMembers(
        Napi::Env env) override {
        interop::FrozenArray<interop::Interface<interop::WGSLStructMember>> out;
        for (auto* member : type_->Members()) {
            out.push_back(interop::WGSLStructMember::Create<Member>(env, program_, member));
        }
        return out;
    }
};

using SamplerType = UnsizedType<interop::WGSLSamplerType, interop::WGSLKind::kSampler>;

using SamplerComparisonType =
    UnsizedType<interop::WGSLSamplerType, interop::WGSLKind::kSamplerComparison>;

class SampledTextureType final : public UnsizedType<interop::WGSLSampledTextureType,
                                                    interop::WGSLKind::kSampledTexture,
                                                    tint::type::SampledTexture> {
  public:
    SampledTextureType(const tint::Program* program, const tint::type::SampledTexture* type)
        : Base(program, type) {}

    interop::GPUTextureViewDimension getDimensions(Napi::Env) override {
        return convert(type_->dim());
    }

    interop::Interface<interop::WGSLSizedType> getSampledType(Napi::Env env) override {
        return createSizedType(env, program_, type_->type());
    }
};

class MultisampledTextureType final : public UnsizedType<interop::WGSLMultisampledTextureType,
                                                         interop::WGSLKind::kMultisampledTexture,
                                                         tint::type::MultisampledTexture> {
  public:
    MultisampledTextureType(const tint::Program* program,
                            const tint::type::MultisampledTexture* type)
        : Base(program, type) {}

    interop::Interface<interop::WGSLSizedType> getSampledType(Napi::Env env) override {
        return createSizedType(env, program_, type_->type());
    }
};

class DepthTextureType final : public UnsizedType<interop::WGSLDepthTextureType,
                                                  interop::WGSLKind::kDepthTexture,
                                                  tint::type::DepthTexture> {
  public:
    DepthTextureType(const tint::Program* program, const tint::type::DepthTexture* type)
        : Base(program, type) {}

    interop::GPUTextureViewDimension getDimensions(Napi::Env) override {
        return convert(type_->dim());
    }
};

using DepthMultisampledTexture = UnsizedType<interop::WGSLDepthMultisampledTexture,
                                             interop::WGSLKind::kDepthMultisampledTexture>;

interop::Interface<interop::WGSLTypeBase> createType(Napi::Env env,
                                                     const tint::Program* program,
                                                     const tint::type::Type* type) {
    return tint::Switch<interop::Interface<interop::WGSLTypeBase>>(
        type,  //
        [&](const tint::type::I32* ty) {
            return interop::WGSLScalarType::Create<I32Type>(env, program, ty);
        },
        [&](const tint::type::U32* ty) {
            return interop::WGSLScalarType::Create<U32Type>(env, program, ty);
        },
        [&](const tint::type::F32* ty) {
            return interop::WGSLScalarType::Create<F32Type>(env, program, ty);
        },
        [&](const tint::type::Bool* ty) {
            return interop::WGSLScalarType::Create<BoolType>(env, program, ty);
        },
        [&](const tint::type::Atomic* ty) {
            return interop::WGSLAtomicType::Create<AtomicType>(env, program, ty);
        },
        [&](const tint::type::Vector* ty) {
            return interop::WGSLVectorType::Create<VectorType>(env, program, ty);
        },
        [&](const tint::type::Matrix* ty) {
            return interop::WGSLMatrixType::Create<MatrixType>(env, program, ty);
        },
        [&](const tint::type::Array* ty) {
            return interop::WGSLArrayType::Create<ArrayType>(env, program, ty);
        },
        [&](const tint::type::Struct* ty) {
            return interop::WGSLStructType::Create<StructType>(env, program, ty);
        },
        [&](const tint::type::Sampler* ty) -> interop::Interface<interop::WGSLTypeBase> {
            if (ty->IsComparison()) {
                return interop::WGSLSamplerType::Create<SamplerComparisonType>(env, program, ty);
            }
            return interop::WGSLSamplerType::Create<SamplerType>(env, program, ty);
        },
        [&](const tint::type::SampledTexture* ty) {
            return interop::WGSLSampledTextureType::Create<SampledTextureType>(env, program, ty);
        },
        [&](const tint::type::DepthTexture* ty) {
            return interop::WGSLDepthTextureType::Create<DepthTextureType>(env, program, ty);
        },
        [&](const tint::type::MultisampledTexture* ty) {
            return interop::WGSLMultisampledTextureType::Create<MultisampledTextureType>(
                env, program, ty);
        },
        [&](const tint::type::DepthMultisampledTexture* ty) {
            return interop::WGSLDepthMultisampledTexture::Create<DepthMultisampledTexture>(
                env, program, ty);
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

}  // namespace

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
