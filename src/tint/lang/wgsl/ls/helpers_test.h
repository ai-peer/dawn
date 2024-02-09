
// Copyright 2024 The Dawn & Tint Authors
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

#ifndef SRC_TINT_LANG_WGSL_LS_HELPERS_TEST_H_
#define SRC_TINT_LANG_WGSL_LS_HELPERS_TEST_H_

#include <vector>

#include "gtest/gtest.h"

#include "src/tint/lang/wgsl/ls/server.h"

namespace tint::wgsl::ls {

template <typename BASE>
class LsTestImpl : public BASE {
  public:
    LsTestImpl() {
        session_.SetSender([&](std::string_view msg) -> langsvr::Result<langsvr::SuccessType> {
            replies_.push_back(std::string(msg));
            return langsvr::Success;
        });
    }

    std::string OpenDocument(std::string_view wgsl) {
        std::string uri = "document-" + std::to_string(next_document_id_++) + ".wgsl";
        langsvr::lsp::TextDocumentDidOpenNotification notification{};
        notification.text_document.text = wgsl;
        notification.text_document.uri = uri;
        auto res = server_.Handle(notification);
        EXPECT_EQ(res, langsvr::Success);
        return uri;
    }

    langsvr::Session session_{};
    Server server_{session_};
    int next_document_id_ = 0;
    std::vector<std::string> replies_;
};

using LsTest = LsTestImpl<testing::Test>;

template <typename T>
using LsTestWithParam = LsTestImpl<testing::TestWithParam<T>>;

}  // namespace tint::wgsl::ls

#endif  // SRC_TINT_LANG_WGSL_LS_HELPERS_TEST_H_
