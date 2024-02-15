// Copyright 2019 The Dawn & Tint Authors
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

#include "dawn/native/ErrorScope.h"

#include <utility>

#include "dawn/common/Assert.h"

namespace dawn::native {

namespace {

wgpu::ErrorType ErrorFilterToErrorType(wgpu::ErrorFilter filter) {
    switch (filter) {
        case wgpu::ErrorFilter::Validation:
            return wgpu::ErrorType::Validation;
        case wgpu::ErrorFilter::OutOfMemory:
            return wgpu::ErrorType::OutOfMemory;
        case wgpu::ErrorFilter::Internal:
            return wgpu::ErrorType::Internal;
    }
    DAWN_UNREACHABLE();
}

}  // namespace

ErrorScope::ErrorScope(wgpu::ErrorFilter errorFilter)
    : mMatchedErrorType(ErrorFilterToErrorType(errorFilter)) {}

ErrorScope::ErrorScope(wgpu::ErrorType error, std::string_view message)
    : mMatchedErrorType(error), mCapturedError(error), mErrorMessage(message) {}

wgpu::ErrorType ErrorScope::GetErrorType() const {
    return mCapturedError;
}

const std::string& ErrorScope::GetErrorMessage() const {
    return mErrorMessage;
}

ErrorScopeStack::ErrorScopeStack() = default;

ErrorScopeStack::~ErrorScopeStack() = default;

const std::vector<ErrorScope>* ErrorScopeStack::GetThreadLocalStack() const {
    return mScopes.Use([&](auto scopes) -> const std::vector<ErrorScope>* {
        auto it = scopes->find(std::this_thread::get_id());
        if (it == scopes->end()) {
            return nullptr;
        }
        return &it->second;
    });
}

std::vector<ErrorScope>* ErrorScopeStack::GetThreadLocalStack() {
    return mScopes.Use([&](auto scopes) -> std::vector<ErrorScope>* {
        auto it = scopes->find(std::this_thread::get_id());
        if (it == scopes->end()) {
            return nullptr;
        }
        return &it->second;
    });
}

void ErrorScopeStack::Push(wgpu::ErrorFilter filter) {
    // Note: operator[] will create the entry in the map if it doesn't exist.
    auto& stack = mScopes->operator[](std::this_thread::get_id());
    stack.push_back(ErrorScope(filter));
}

ErrorScope ErrorScopeStack::Pop() {
    std::vector<ErrorScope>* stack = GetThreadLocalStack();
    DAWN_ASSERT(stack != nullptr);
    DAWN_ASSERT(!stack->empty());
    ErrorScope scope = std::move(stack->back());
    stack->pop_back();
    return scope;
}

bool ErrorScopeStack::Empty() const {
    const std::vector<ErrorScope>* stack = GetThreadLocalStack();
    if (!stack) {
        return true;
    }
    return stack->empty();
}

bool ErrorScopeStack::HandleError(wgpu::ErrorType type, const char* message) {
    std::vector<ErrorScope>* stack = GetThreadLocalStack();
    if (stack == nullptr) {
        return false;
    }

    for (auto it = stack->rbegin(); it != stack->rend(); ++it) {
        if (it->mMatchedErrorType != type) {
            // Error filter does not match. Move on to the next scope.
            continue;
        }

        // Filter matches.
        // Record the error if the scope doesn't have one yet.
        if (it->mCapturedError == wgpu::ErrorType::NoError) {
            it->mCapturedError = type;
            it->mErrorMessage = message;
        }

        if (type == wgpu::ErrorType::DeviceLost) {
            if (it->mCapturedError != wgpu::ErrorType::DeviceLost) {
                // DeviceLost overrides any other error that is not a DeviceLost.
                it->mCapturedError = type;
                it->mErrorMessage = message;
            }
        } else {
            // Errors that are not device lost are captured and stop propogating.
            return true;
        }
    }

    // The error was not captured.
    return false;
}

}  // namespace dawn::native
