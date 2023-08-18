// Copyright 2023 The Dawn Authors
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

#ifndef SRC_DAWN_NATIVE_CONSTCONTENT_H_
#define SRC_DAWN_NATIVE_CONSTCONTENT_H_

#include <utility>

#define DAWN_INTERNAL_CONTENT_FUNC_HEADER(type, name, args) type name args const;

#define DAWN_INTERNAL_CONTENT_PROXY_FUNC_INLINE(type, name, ...)      \
    type name(auto&&... args) const {                                 \
        ASSERT(!IsError());                                           \
        return mContents.name(std::forward<decltype(args)>(args)...); \
    }

#define DAWN_CONTENT_FUNC_HEADERS(FUNCS) FUNCS(DAWN_INTERNAL_CONTENT_FUNC_HEADER)
#define DAWN_CONTENT_PROXY_FUNCS(FUNCS) FUNCS(DAWN_INTERNAL_CONTENT_PROXY_FUNC_INLINE)

#endif  // SRC_DAWN_NATIVE_CONSTCONTENT_H_
