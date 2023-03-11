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

#include "tint/interp/memory.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

#include "tint/interp/invocation.h"
#include "tint/interp/shader_executor.h"
#include "tint/lang/core/constant/scalar.h"
#include "tint/lang/core/type/array.h"
#include "tint/lang/core/type/atomic.h"
#include "tint/lang/core/type/matrix.h"
#include "tint/lang/core/type/struct.h"
#include "tint/lang/core/type/vector.h"
#include "tint/lang/wgsl/program/program_builder.h"
#include "tint/utils/rtti/switch.h"
#include "tint/utils/text/string_stream.h"

namespace tint::interp {

Memory::Memory(uint64_t size) {
    data_.assign(size, 0);
}

Memory::~Memory() {}

MemoryView* Memory::CreateView(ShaderExecutor* executor,
                               core::AddressSpace addrspace,
                               const core::type::Type* type,
                               uint64_t offset,
                               uint64_t size,
                               Source source) {
    TINT_ASSERT(size >= type->Size());
    bool valid = offset + size <= Size();
    return executor->MemoryViews().Create(executor, this, nullptr, addrspace, type, offset, size,
                                          source, valid);
}

MemoryView* Memory::CreateView(ShaderExecutor* executor,
                               core::AddressSpace addrspace,
                               const core::type::Type* type,
                               Source source) {
    return CreateView(executor, addrspace, type, 0, Size(), source);
}

void Memory::CopyFrom(uint64_t dst_offset, Memory& src, uint64_t src_offset, uint64_t size) {
    if (dst_offset + size > Size()) {
        // TODO(jrprice): Produce error for OOB copies.
        return;
    }
    if (src_offset + size > src.Size()) {
        // TODO(jrprice): Produce error for OOB copies.
        return;
    }
    memcpy(Data() + dst_offset, src.Data() + src_offset, size);
}

template <typename T>
T Memory::AtomicLoad(uint64_t offset) {
    // TODO(jrprice): Will need a mutex here if we ever parallelize the interpreter.
    T result;
    Load(&result, offset);
    return result;
}

template <typename T>
void Memory::AtomicStore(uint64_t offset, T value) {
    // TODO(jrprice): Will need a mutex here if we ever parallelize the interpreter.
    Store(&value, offset);
}

template <typename T>
T Memory::AtomicRMW(uint64_t offset, AtomicOp op, T value) {
    // TODO(jrprice): Will need a mutex here if we ever parallelize the interpreter.
    T old_value;
    T new_value = 0;
    Load(&old_value, offset);
    switch (op) {
        case AtomicOp::kAdd:
            new_value = old_value + value;
            break;
        case AtomicOp::kSub:
            new_value = old_value - value;
            break;
        case AtomicOp::kMax:
            new_value = std::max(old_value, value);
            break;
        case AtomicOp::kMin:
            new_value = std::min(old_value, value);
            break;
        case AtomicOp::kAnd:
            new_value = old_value & value;
            break;
        case AtomicOp::kOr:
            new_value = old_value | value;
            break;
        case AtomicOp::kXor:
            new_value = old_value ^ value;
            break;
        case AtomicOp::kXchg:
            new_value = value;
            break;
    }
    Store(&new_value, offset);
    return old_value;
}

template <typename T>
T Memory::AtomicCompareExchange(uint64_t offset, T cmp, T value, bool& exchanged) {
    // TODO(jrprice): Will need a mutex here if we ever parallelize the interpreter.
    T result;
    Load(&result, offset);
    if (result == cmp) {
        exchanged = true;
        Store(&value, offset);
    } else {
        exchanged = false;
    }
    return result;
}

MemoryView::MemoryView(ShaderExecutor* executor,
                       Memory* memory,
                       MemoryView* parent,
                       core::AddressSpace addrspace,
                       const core::type::Type* type,
                       uint64_t offset,
                       uint64_t size,
                       tint::Source source,
                       bool valid)
    : executor_(executor),
      memory_(memory),
      parent_(parent),
      address_space_(addrspace),
      type_(type->UnwrapRef()),
      offset_(offset),
      size_(size),
      source_(source),
      is_valid_(valid) {}

const core::constant::Value* MemoryView::Load() {
    if (!is_valid_) {
        ReportOutOfBounds("loading from an out-of-bounds memory view");
        return executor_->ConstEval().Zero(type_, {}, source_).Get();
    }
    auto* result = Load(type_, offset_);
    executor_->ReportMemoryLoad(this);
    return result;
}

const core::constant::Value* MemoryView::Load(const core::type::Type* type, uint64_t offset) {
    // Helper that checks for non-finite floating point values loaded by this function.
    // Raises a diagnostic for non-finite values, and then sets the value to zero.
    auto check_fp_value = [&](float& value) {
        if (!std::isfinite(value)) {
            tint::Source source{};
            if (auto* invocation = executor_->CurrentInvocation()) {
                if (auto* expr = invocation->CurrentExpression()) {
                    source = expr->source;
                } else if (auto* stmt = invocation->CurrentStatement()) {
                    source = stmt->source;
                }
            }
            diag::List list;
            list.AddWarning(diag::System::Interpreter, source)
                << "loading a non-finite " << type->FriendlyName() << " value (" << value << ")";
            executor_->ReportError(std::move(list));
            value = 0;
        }
    };

    auto& ce = executor_->ConstEval();
    return Switch(
        type,  //
        [&](const core::type::Bool*) {
            uint32_t value;
            memory_->Load(&value, offset);
            return executor_->Builder().constants.Get(value != 0u);
        },
        [&](const core::type::F32*) {
            core::f32 value;
            memory_->Load(&value.value, offset);
            check_fp_value(value.value);
            return executor_->Builder().constants.Get(value);
        },
        [&](const core::type::F16*) {
            // Load the bit representation and convert it to an F16 value.
            uint16_t bits;
            memory_->Load(&bits, offset);
            core::f16 value = core::f16::FromBits(bits);
            check_fp_value(value.value);
            return executor_->Builder().constants.Get(value);
        },
        [&](const core::type::I32*) {
            core::i32 value;
            memory_->Load(&value.value, offset);
            return executor_->Builder().constants.Get(value);
        },
        [&](const core::type::U32*) {
            core::u32 value;
            memory_->Load(&value.value, offset);
            return executor_->Builder().constants.Get(value);
        },
        [&](const core::type::Vector* vec) -> const core::constant::Value* {
            tint::Vector<const core::constant::Value*, 4> elements;
            for (uint32_t i = 0; i < vec->Width(); i++) {
                elements.Push(Load(vec->type(), offset + i * vec->type()->Size()));
            }
            return ce.VecInitS(vec, elements, {}).Get();
        },
        [&](const core::type::Matrix* mat) -> const core::constant::Value* {
            tint::Vector<const core::constant::Value*, 4> columns;
            for (uint32_t i = 0; i < mat->columns(); i++) {
                columns.Push(Load(mat->ColumnType(), offset + i * mat->ColumnStride()));
            }
            return ce.VecInitS(mat, columns, {}).Get();
        },
        [&](const core::type::Array* arr) -> const core::constant::Value* {
            uint64_t count;
            if (arr->Count()->Is<core::type::RuntimeArrayCount>()) {
                count = (size_ - offset) / arr->Stride();
            } else if (arr->Count()->Is<sem::NamedOverrideArrayCount>() ||
                       arr->Count()->Is<sem::UnnamedOverrideArrayCount>()) {
                TINT_ASSERT(offset == 0);
                count = size_ / arr->Stride();
            } else if (arr->ConstantCount()) {
                count = arr->ConstantCount().value();
            } else {
                executor_->ReportFatalError("unhandled non-constant size array in memory load");
                return nullptr;
            }
            tint::Vector<const core::constant::Value*, 8> elements;
            for (uint32_t i = 0; i < count; i++) {
                elements.Push(Load(arr->ElemType(), offset + i * arr->Stride()));
            }
            return ce.ArrayOrStructCtor(arr, elements).Get();
        },
        [&](const core::type::Struct* str) {
            tint::Vector<const core::constant::Value*, 8> elements;
            for (auto* member : str->Members()) {
                elements.Push(Load(member->Type(), offset + member->Offset()));
            }
            return ce.ArrayOrStructCtor(str, elements).Get();
        },
        [&](const core::type::Atomic* a) { return Load(a->Type(), offset); },
        [&](Default) -> core::constant::Value* {
            executor_->ReportFatalError("unhandled type in memory load");
            return nullptr;
        });
}

void MemoryView::Store(const core::constant::Value* value) {
    if (!is_valid_) {
        ReportOutOfBounds("storing to an out-of-bounds memory view");
        return;
    }
    TINT_ASSERT(value->Type() == type_);
    Store(value, offset_);
    executor_->ReportMemoryStore(this);
}

void MemoryView::Store(const core::constant::Value* value, uint64_t offset) {
    Switch(
        value->Type(),  //
        [&](const core::type::Bool*) {
            uint32_t v = value->ValueAs<uint32_t>();
            memory_->Store(&v, offset);
        },
        [&](const core::type::F32*) {
            float v = value->ValueAs<float>();
            memory_->Store(&v, offset);
        },
        [&](const core::type::F16*) {
            // Store the bit representation of the F16 value.
            uint16_t v = core::f16(value->ValueAs<float>()).BitsRepresentation();
            memory_->Store(&v, offset);
        },
        [&](const core::type::I32*) {
            int32_t v = value->ValueAs<int32_t>();
            memory_->Store(&v, offset);
        },
        [&](const core::type::U32*) {
            uint32_t v = value->ValueAs<uint32_t>();
            memory_->Store(&v, offset);
        },
        [&](const core::type::Vector* vec) {
            for (uint32_t i = 0; i < vec->Width(); i++) {
                Store(value->Index(i), offset + i * vec->type()->Size());
            }
        },
        [&](const core::type::Matrix* mat) {
            for (uint32_t i = 0; i < mat->columns(); i++) {
                Store(value->Index(i), offset + i * mat->ColumnStride());
            }
        },
        [&](const core::type::Array* arr) {
            if (!arr->ConstantCount()) {
                executor_->ReportFatalError("unhandled non-constant size array in memory store");
                return;
            }
            for (uint32_t i = 0; i < arr->ConstantCount(); i++) {
                Store(value->Index(i), offset + i * arr->Stride());
            }
        },
        [&](const core::type::Struct* str) {
            for (auto* member : str->Members()) {
                Store(value->Index(member->Index()), offset + member->Offset());
            }
        },
        [&](Default) { executor_->ReportFatalError("unhandled type in memory store"); });
}

MemoryView* MemoryView::CreateSubview(const core::type::Type* type,
                                      uint64_t offset,
                                      uint64_t size,
                                      tint::Source source) {
    bool valid = is_valid_ && (offset + size <= size_);
    return executor_->MemoryViews().Create(executor_, memory_, this, address_space_, type,
                                           offset_ + offset, size, source, valid);
}

namespace {
// Helper to handle type selection for atomic operations.
// This will call the function `f` and pass it a zero-value with the C++ type that corresponds to
// the type of the atomic operation, and then construct a core::constant::Value from the result.
template <typename F>
const core::constant::Value* AtomicDispatch(ShaderExecutor& executor,
                                            const core::type::Type* ty,
                                            F&& f) {
    return Switch(
        ty,  //
        [&](const core::type::I32*) {
            return executor.Builder().constants.Get(core::i32(f(int32_t(0))));
        },
        [&](const core::type::U32*) {
            return executor.Builder().constants.Get(core::u32(f(uint32_t(0))));
        },
        [&](Default) -> core::constant::Value* {
            executor.ReportFatalError("unhandled atomic type");
            return nullptr;
        });
}
}  // namespace

const core::constant::Value* MemoryView::AtomicLoad() {
    auto* atomic = type_->As<core::type::Atomic>();
    TINT_ASSERT(atomic);
    if (!is_valid_) {
        ReportOutOfBounds("atomic operation on an out-of-bounds memory view");
        return executor_->ConstEval().Zero(atomic->Type(), {}, source_).Get();
    }
    return AtomicDispatch(*executor_, atomic->Type(),
                          [&](auto T) {  //
                              return memory_->AtomicLoad<decltype(T)>(offset_);
                          });
}

void MemoryView::AtomicStore(const core::constant::Value* value) {
    auto* atomic = type_->As<core::type::Atomic>();
    TINT_ASSERT(atomic);
    if (!is_valid_) {
        ReportOutOfBounds("atomic operation on an out-of-bounds memory view");
        return;
    }
    AtomicDispatch(*executor_, atomic->Type(), [&](auto T) {
        memory_->AtomicStore(offset_, value->ValueAs<decltype(T)>());
        return 0;
    });
}

const core::constant::Value* MemoryView::AtomicRMW(AtomicOp op,
                                                   const core::constant::Value* value) {
    auto* atomic = type_->As<core::type::Atomic>();
    TINT_ASSERT(atomic);
    if (!is_valid_) {
        ReportOutOfBounds("atomic operation on an out-of-bounds memory view");
        return executor_->ConstEval().Zero(atomic->Type(), {}, source_).Get();
    }
    return AtomicDispatch(*executor_, atomic->Type(), [&](auto T) {
        return memory_->AtomicRMW(offset_, op, value->ValueAs<decltype(T)>());
    });
}

const core::constant::Value* MemoryView::AtomicCompareExchange(const core::constant::Value* cmp,
                                                               const core::constant::Value* value,
                                                               bool& exchanged) {
    auto* atomic = type_->As<core::type::Atomic>();
    TINT_ASSERT(atomic);
    if (!is_valid_) {
        ReportOutOfBounds("atomic operation on an out-of-bounds memory view");
        return executor_->ConstEval().Zero(atomic->Type(), {}, source_).Get();
    }
    return AtomicDispatch(*executor_, atomic->Type(), [&](auto T) {
        return memory_->AtomicCompareExchange(offset_, cmp->ValueAs<decltype(T)>(),
                                              value->ValueAs<decltype(T)>(), exchanged);
    });
}

void MemoryView::ReportOutOfBounds(std::string msg) {
    diag::List list;

    // Report the error on the expression that caused it.
    {
        tint::Source source{};
        if (auto* invocation = executor_->CurrentInvocation()) {
            if (auto* expr = invocation->CurrentExpression()) {
                source = expr->source;
            } else if (auto* stmt = invocation->CurrentStatement()) {
                source = stmt->source;
            }
        }
        list.AddWarning(diag::System::Interpreter, source) << msg;
    }

    // Find the first view that was invalid in this view's parent chain.
    // Also find the root memory view, which will identify the original declaration in the shader.
    MemoryView* root = this;
    MemoryView* first_invalid = this;
    while (root->parent_) {
        if (!root->is_valid_) {
            first_invalid = root;
        }
        root = root->parent_;
    }

    // Show the base allocation that we are accessing.
    list.AddNote(diag::System::Interpreter, root->source_)
        << "accessing " << root->Size() << " byte allocation in the " << root->AddressSpace()
        << " address space";

    // Show where the invalid view was created.
    list.AddNote(diag::System::Interpreter, first_invalid->source_)
        << "created a " << first_invalid->Size() << " byte memory view at an offset of "
        << first_invalid->offset_ << " bytes";

    executor_->ReportError(std::move(list));
}

}  // namespace tint::interp
