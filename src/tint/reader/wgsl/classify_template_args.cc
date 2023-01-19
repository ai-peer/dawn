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

#include "src/tint/reader/wgsl/classify_template_args.h"

#include <vector>

#include "src/tint/debug.h"
#include "src/tint/utils/vector.h"

namespace tint::reader::wgsl {

void ClassifyTemplateArguments(std::vector<Token>& tokens) {
    const size_t count = tokens.size();

    int depth = 0;

    struct StackEntry {
        Token* token;
        int depth;
    };
    utils::Vector<StackEntry, 16> stack;
    for (size_t i = 0; i < count - 1; i++) {
        switch (tokens[i].type()) {
            case Token::Type::kIdentifier:
            case Token::Type::kArray:
            case Token::Type::kBitcast:
            case Token::Type::kVec2:
            case Token::Type::kVec3:
            case Token::Type::kVec4:
            case Token::Type::kMat2x2:
            case Token::Type::kMat2x3:
            case Token::Type::kMat2x4:
            case Token::Type::kMat3x2:
            case Token::Type::kMat3x3:
            case Token::Type::kMat3x4:
            case Token::Type::kMat4x2:
            case Token::Type::kMat4x3:
            case Token::Type::kMat4x4:
            case Token::Type::kPtr:
            case Token::Type::kTextureSampled1d:
            case Token::Type::kTextureSampled2d:
            case Token::Type::kTextureSampled2dArray:
            case Token::Type::kTextureSampled3d:
            case Token::Type::kTextureSampledCube:
            case Token::Type::kTextureSampledCubeArray:
            case Token::Type::kTextureMultisampled2d:
            case Token::Type::kTextureStorage1d:
            case Token::Type::kTextureStorage2d:
            case Token::Type::kTextureStorage2dArray:
            case Token::Type::kTextureStorage3d: {
                auto& next = tokens[i + 1];
                if (next.type() == Token::Type::kLessThan) {
                    stack.Push(StackEntry{&tokens[i + 1], depth});
                    i++;  // Skip the '<'
                }
                break;
            }
            case Token::Type::kGreaterThan:  // '>'
                if (!stack.IsEmpty() && stack.Back().depth == depth) {
                    auto& next = tokens[i + 1];
                    // Split '>>', '>>='
                    if (next.type() == Token::Type::kParenLeft ||    // '('
                        next.type() == Token::Type::kPeriod ||       // '.'
                        next.type() == Token::Type::kGreaterThan) {  // '>'
                        stack.Pop().token->SetType(Token::Type::kTemplateArgsLeft);
                        tokens[i].SetType(Token::Type::kTemplateArgsRight);
                    }
                }
                break;

            case Token::Type::kShiftRight:  // '>>'
                if (!stack.IsEmpty() && stack.Back().depth == depth) {
                    auto& next = tokens[i + 1];
                    // Split '>>'
                    TINT_ASSERT(Reader, next.type() == Token::Type::kPlaceholder);
                    next.SetType(Token::Type::kGreaterThan);

                    stack.Pop().token->SetType(Token::Type::kTemplateArgsLeft);
                    tokens[i].SetType(Token::Type::kTemplateArgsRight);
                }
                break;

            case Token::Type::kParenLeft:    // '('
            case Token::Type::kBracketLeft:  // '['
                depth++;
                break;
            case Token::Type::kParenRight:    // ')'
            case Token::Type::kBracketRight:  // ']'
                depth--;
                break;
            case Token::Type::kSemicolon:  // ';'
            case Token::Type::kBraceLeft:  // '{'
                depth = 0;
                stack.Clear();
                break;

            default:
                break;
        }
    }
}

}  // namespace tint::reader::wgsl
