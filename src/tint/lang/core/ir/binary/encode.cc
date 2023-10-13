// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/tint/lang/core/ir/binary/encode.h"

#include "src/tint/lang/core/ir/binary/ir.pb.h"
#include "src/tint/lang/core/ir/module.h"
#include "src/tint/lang/core/type/bool.h"
#include "src/tint/lang/core/type/f32.h"
#include "src/tint/lang/core/type/i32.h"
#include "src/tint/lang/core/type/u32.h"
#include "src/tint/utils/rtti/switch.h"

namespace tint::core::ir::binary {
namespace {
struct Encoder {
    const Module& mod_in;
    pb::Module& mod_out;
    Hashmap<const core::type::Type*, uint32_t, 32> types;

    void Encode() {
        for (auto* fn_in : mod_in.functions) {
            auto& fn_out = *mod_out.add_functions();
            if (auto name = mod_in.NameOf(fn_in)) {
                fn_out.set_name(name.Name());
            }
            fn_out.set_return_type(Type(fn_in->ReturnType()));
            if (fn_in->Stage() != Function::PipelineStage::kUndefined) {
                fn_out.set_pipeline_stage(PipelineStage(fn_in->Stage()));
            }
        }
    }

    pb::PipelineStage PipelineStage(Function::PipelineStage stage) {
        switch (stage) {
            case Function::PipelineStage::kCompute:
                return pb::PipelineStage::Compute;
            case Function::PipelineStage::kFragment:
                return pb::PipelineStage::Fragment;
            case Function::PipelineStage::kVertex:
                return pb::PipelineStage::Vertex;
            default:
                TINT_ICE() << "unhandled PipelineStage: " << stage;
                return pb::PipelineStage::Compute;
        }
    }

    uint32_t Type(const core::type::Type* type) {
        if (type == nullptr) {
            return 0;
        }
        return types.GetOrCreate(type, [&]() -> uint32_t {
            tint::Switch(
                type,  //
                [&](const core::type::Bool*) {
                    mod_out.add_types()->set_scalar(pb::ScalarType::bool_);
                },
                [&](const core::type::I32*) {
                    mod_out.add_types()->set_scalar(pb::ScalarType::i32);
                },
                [&](const core::type::U32*) {
                    mod_out.add_types()->set_scalar(pb::ScalarType::u32);
                },
                [&](const core::type::F32*) {
                    mod_out.add_types()->set_scalar(pb::ScalarType::f32);
                },
                [&](Default) { TINT_ICE() << "unhandled type: " << type->FriendlyName(); });
            return static_cast<uint32_t>(types.Count());
        });
    }
};

}  // namespace

Result<Vector<std::byte, 0>> Encode(const Module& mod_in) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    pb::Module mod_out;
    Encoder{mod_in, mod_out}.Encode();

    Vector<std::byte, 0> buffer;
    size_t len = mod_out.ByteSize();
    buffer.Resize(len);
    if (len > 0) {
        if (!mod_out.SerializeToArray(&buffer[0], len)) {
            return Failure{"failed to serialize protobuf"};
        }
    }
    return buffer;
}

}  // namespace tint::core::ir::binary
