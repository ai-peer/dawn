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

#include "src/tint/lang/core/ir/binary/decode.h"

#include "src/tint/lang/core/ir/binary/ir.pb.h"
#include "src/tint/lang/core/ir/builder.h"
#include "src/tint/lang/core/ir/module.h"

using namespace tint::core::fluent_types;  // NOLINT

namespace tint::core::ir::binary {
namespace {

struct Decoder {
    pb::Module& mod_in;
    Module& mod_out;
    Builder b{mod_out};
    Hashmap<uint32_t, const core::type::Type*, 32> types;

    void Decode() {
        for (auto& fn_in : mod_in.functions()) {
            auto* return_type = Type(fn_in.return_type());
            auto stage = Function::PipelineStage::kUndefined;
            if (fn_in.has_pipeline_stage()) {
                stage = PipelineStage(fn_in.pipeline_stage());
            }
            b.Function(fn_in.name(), return_type, stage);
        }
    }

    Function::PipelineStage PipelineStage(pb::PipelineStage stage) {
        switch (stage) {
            case pb::PipelineStage::Compute:
                return Function::PipelineStage::kCompute;
            case pb::PipelineStage::Fragment:
                return Function::PipelineStage::kFragment;
            case pb::PipelineStage::Vertex:
                return Function::PipelineStage::kVertex;
            default:
                TINT_ICE() << "unhandled PipelineStage: " << stage;
                return Function::PipelineStage::kCompute;
        }
    }

    const core::type::Type* Type(uint32_t id) {
        if (id == 0) {
            return nullptr;
        }
        return types.GetOrCreate(id, [&]() -> const core::type::Type* {
            auto& ty_in = mod_in.types()[id - 1];
            switch (ty_in.kind_case()) {
                case pb::TypeDecl::KindCase::kScalar:
                    switch (ty_in.scalar()) {
                        case pb::ScalarType::bool_:
                            return mod_out.Types().Get<bool>();
                        case pb::ScalarType::i32:
                            return mod_out.Types().Get<i32>();
                        case pb::ScalarType::u32:
                            return mod_out.Types().Get<u32>();
                        case pb::ScalarType::f32:
                            return mod_out.Types().Get<f32>();
                        default:
                            TINT_ICE() << "invalid ScalarType: " << ty_in.scalar();
                            return nullptr;
                    }
                case pb::TypeDecl::KindCase::KIND_NOT_SET:
                    TINT_ICE() << "invalid TypeDecl.kind";
                    return nullptr;
            }
        });
    }
};

}  // namespace

Result<Module> Decode(Slice<const std::byte> encoded) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    pb::Module mod_in;
    if (!mod_in.ParseFromArray(encoded.data, encoded.len)) {
        return Failure{"failed to deserialize protobuf"};
    }

    Module mod_out;
    Decoder{mod_in, mod_out}.Decode();

    return mod_out;
}

}  // namespace tint::core::ir::binary
