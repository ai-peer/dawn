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

#ifndef DAWNNATIVE_CHAIN_UTILS_H_
#define DAWNNATIVE_CHAIN_UTILS_H_

#include "common/Preprocessor.h"
#include "dawn_native/dawn_platform.h"
#include "dawn_native/Error.h"

namespace dawn_native {
    {% for value in types["s type"].values %}
        {% if value.valid %}
            void FindInChain(const ChainedStruct* chain, const {{as_cppEnum(value.name)}}** out);
        {% endif %}
    {% endfor %}

    // Validate that a single chained struct in |args| is non-null.
    template <typename... Args>
    MaybeError ValidateSingleChainedStruct(const Args*... args) {
        bool found = false;
        for (const ChainedStruct* arg : {static_cast<const ChainedStruct*>(args)...}) {
            if (DAWN_UNLIKELY(arg != nullptr && found)) {
                return DAWN_VALIDATION_ERROR("Expected a single chained struct.");
            }
            if (arg) {
                found = true;
            }
        }
        if (DAWN_UNLIKELY(!found)) {
            return DAWN_VALIDATION_ERROR("Expected a single chained struct.");
        }
        return {};
    }

#define DEFINE_UNPACKED_CHAINED_STRUCT_HOLDER_MEMBER(name) const ::dawn_native::name * name;

#define DAWN_TRY_UNPACK_CHAINED_STRUCTS_CASE_POPULATE_STRUCT_PTR(name)  \
case wgpu::SType::name: {                                               \
    if (DAWN_UNLIKELY(result->name != nullptr)) {                       \
        return DAWN_VALIDATION_ERROR("Duplicate sType " #name);         \
    }                                                                   \
    result->name = static_cast<const ::dawn_native::name *>(chain);     \
    break;                                                              \
}

#define DEFINE_UNPACKED_CHAINED_STRUCT_HOLDER(Name, ...) \
struct Name {                                            \
    DAWN_PP_FOR_EACH(                                    \
        DEFINE_UNPACKED_CHAINED_STRUCT_HOLDER_MEMBER,    \
        __VA_ARGS__)                                     \
}

// Unpacks chained structs into |outPtr|. Validates that there
// are no sTypes not in the expected list, and that there are no duplicate sTypes.
#define DAWN_TRY_ASSIGN_UNPACK_CHAINED_STRUCTS(outPtr, chainIn, ...)          \
    do {                                                                      \
        auto* result = outPtr;                                                \
        const ::dawn_native::ChainedStruct* chain = chainIn;                  \
        for (; chain; chain = chain->nextInChain) {                           \
            using namespace dawn_native;                                      \
            switch (chain->sType) {                                           \
                DAWN_PP_FOR_EACH(                                             \
                    DAWN_TRY_UNPACK_CHAINED_STRUCTS_CASE_POPULATE_STRUCT_PTR, \
                    __VA_ARGS__)                                              \
                default:                                                      \
                    return DAWN_VALIDATION_ERROR("unsupported sType");        \
            }                                                                 \
        }                                                                     \
    } while(false)

#define DAWN_TRY_UNPACK_CHAINED_STRUCTS(outVarName, chainIn, ...)             \
    DEFINE_UNPACKED_CHAINED_STRUCT_HOLDER(, __VA_ARGS__) outVarName {};       \
    DAWN_TRY_ASSIGN_UNPACK_CHAINED_STRUCTS(&outVarName, chainIn, __VA_ARGS__)

#define DAWN_UNPACK_CHAINED_STRUCTS(chainIn, ...)                            \
[&]() {                                                                      \
    DEFINE_UNPACKED_CHAINED_STRUCT_HOLDER(Out, __VA_ARGS__) out = {};        \
    ::dawn_native::MaybeError err = [&]() -> ::dawn_native::MaybeError {     \
        DAWN_TRY_ASSIGN_UNPACK_CHAINED_STRUCTS(&out, chainIn, __VA_ARGS__);  \
        return {};                                                           \
    }();                                                                     \
    if (DAWN_UNLIKELY(err.IsError())) {                                      \
        return Result<Out, ::dawn_native::ErrorData>(err.AcquireError());    \
    }                                                                        \
    return Result<Out, ::dawn_native::ErrorData>(std::move(out));            \
}()

}  // namespace dawn_native

#endif  // DAWNNATIVE_CHAIN_UTILS_H_
