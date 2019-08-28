// Copyright 2019 The Dawn Authors
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

#include "dawn_native/ErrorScope.h"

#include "common/Assert.h"
#include "dawn_native/ErrorData.h"

namespace dawn_native {

    ErrorScope::ErrorScope() = default;

    ErrorScope::ErrorScope(dawn::ErrorFilter errorFilter, Ref<ErrorScope> parent)
        : RefCounted(), mErrorFilter(errorFilter), mParent(std::move(parent)) {
        ASSERT(mParent.Get() != nullptr);
    }

    ErrorScope::~ErrorScope() {
        if (mCallback == nullptr || IsRoot()) {
            return;
        }
        mCallback(static_cast<DawnErrorType>(mErrorType), mErrorMessage.c_str(), mUserdata);
    }

    void ErrorScope::SetCallback(dawn::ErrorCallback callback, void* userdata) {
        mCallback = callback;
        mUserdata = userdata;
    }

    ErrorScope* ErrorScope::GetParent() {
        return mParent.Get();
    }

    bool ErrorScope::IsRoot() const {
        return mParent.Get() == nullptr;
    }

    void ErrorScope::HandleError(dawn::ErrorType type, const char* message) {
        HandleErrorImpl(this, type, message);
    }

    void ErrorScope::HandleError(ErrorData* error) {
        ASSERT(error != nullptr);
        HandleErrorImpl(this, error->GetType(), error->GetMessage().c_str());
    }

    // static
    void ErrorScope::HandleErrorImpl(ErrorScope* scope, dawn::ErrorType type, const char* message) {
        ErrorScope* currentScope = scope;
        bool consumed = false;
        while (!consumed) {
            ASSERT(currentScope != nullptr);

            // The root error scope captures all uncaptured errors.
            if (currentScope->IsRoot()) {
                if (currentScope->mCallback) {
                    currentScope->mCallback(static_cast<DawnErrorType>(type), message,
                                            currentScope->mUserdata);
                }
                return;
            }

            switch (type) {
                case dawn::ErrorType::NoError:
                    UNREACHABLE();
                    return;

                case dawn::ErrorType::Validation:
                    // Error filter does not match. Move on to the next scope.
                    if (currentScope->mErrorFilter != dawn::ErrorFilter::Validation) {
                        currentScope = currentScope->GetParent();
                        continue;
                    }
                    consumed = true;
                    break;

                case dawn::ErrorType::OutOfMemory:
                    // Error filter does not match. Move on to the next scope.
                    if (currentScope->mErrorFilter != dawn::ErrorFilter::OutOfMemory) {
                        currentScope = currentScope->GetParent();
                        continue;
                    }
                    consumed = true;
                    break;

                // Unknown and DeviceLost are fatal. All error scopes capture them.
                // |consumed| is false because these should bubble to all scopes.
                case dawn::ErrorType::Unknown:
                case dawn::ErrorType::DeviceLost:
                    consumed = false;
                    break;

                default:
                    UNREACHABLE();
            }

            // Record the error if the scope doesn't have one yet.
            if (currentScope->mErrorType == dawn::ErrorType::NoError) {
                currentScope->mErrorType = type;
                currentScope->mErrorMessage = message;
            }

            // The current scope did not consume the error. Move on to the next scope.
            if (!consumed) {
                currentScope = currentScope->GetParent();
            }
        }
    }

}  // namespace dawn_native
