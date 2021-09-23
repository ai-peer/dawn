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

#include "src/dawn_node/binding/Errors.h"

namespace wgpu { namespace binding {

    namespace {
        const char* kHierarchyRequestError = "HierarchyRequestError";
        const char* kWrongDocumentError = "WrongDocumentError";
        const char* kInvalidCharacterError = "InvalidCharacterError";
        const char* kNoModificationAllowedError = "NoModificationAllowedError";
        const char* kNotFoundError = "NotFoundError";
        const char* kNotSupportedError = "NotSupportedError";
        const char* kInUseAttributeError = "InUseAttributeError";
        const char* kInvalidStateError = "InvalidStateError";
        const char* kSyntaxError = "SyntaxError";
        const char* kInvalidModificationError = "InvalidModificationError";
        const char* kNamespaceError = "NamespaceError";
        const char* kSecurityError = "SecurityError";
        const char* kNetworkError = "NetworkError";
        const char* kAbortError = "AbortError";
        const char* kURLMismatchError = "URLMismatchError";
        const char* kQuotaExceededError = "QuotaExceededError";
        const char* kTimeoutError = "TimeoutError";
        const char* kInvalidNodeTypeError = "InvalidNodeTypeError";
        const char* kDataCloneError = "DataCloneError";
        const char* kEncodingError = "EncodingError";
        const char* kNotReadableError = "NotReadableError";
        const char* kUnknownError = "UnknownError";
        const char* kConstraintError = "ConstraintError";
        const char* kDataError = "DataError";
        const char* kTransactionInactiveError = "TransactionInactiveError";
        const char* kReadOnlyError = "ReadOnlyError";
        const char* kVersionError = "VersionError";
        const char* kOperationError = "OperationError";
        const char* kNotAllowedError = "NotAllowedError";

        static Napi::Error New(Napi::Env env,
                               std::string name,
                               std::string message = {},
                               unsigned short code = 0) {
            auto err = Napi::Error::New(env);
            err.Set("name", name);
            err.Set("message", message.empty() ? name : message);
            err.Set("code", static_cast<double>(code));
            return err;
        }

    }  // namespace

    Napi::Error Errors::HierarchyRequestError(Napi::Env env) {
        return New(env, kHierarchyRequestError);
    }

    Napi::Error Errors::WrongDocumentError(Napi::Env env) {
        return New(env, kWrongDocumentError);
    }

    Napi::Error Errors::InvalidCharacterError(Napi::Env env) {
        return New(env, kInvalidCharacterError);
    }

    Napi::Error Errors::NoModificationAllowedError(Napi::Env env) {
        return New(env, kNoModificationAllowedError);
    }

    Napi::Error Errors::NotFoundError(Napi::Env env) {
        return New(env, kNotFoundError);
    }

    Napi::Error Errors::NotSupportedError(Napi::Env env) {
        return New(env, kNotSupportedError);
    }

    Napi::Error Errors::InUseAttributeError(Napi::Env env) {
        return New(env, kInUseAttributeError);
    }

    Napi::Error Errors::InvalidStateError(Napi::Env env) {
        return New(env, kInvalidStateError);
    }

    Napi::Error Errors::SyntaxError(Napi::Env env) {
        return New(env, kSyntaxError);
    }

    Napi::Error Errors::InvalidModificationError(Napi::Env env) {
        return New(env, kInvalidModificationError);
    }

    Napi::Error Errors::NamespaceError(Napi::Env env) {
        return New(env, kNamespaceError);
    }

    Napi::Error Errors::SecurityError(Napi::Env env) {
        return New(env, kSecurityError);
    }

    Napi::Error Errors::NetworkError(Napi::Env env) {
        return New(env, kNetworkError);
    }

    Napi::Error Errors::AbortError(Napi::Env env) {
        return New(env, kAbortError);
    }

    Napi::Error Errors::URLMismatchError(Napi::Env env) {
        return New(env, kURLMismatchError);
    }

    Napi::Error Errors::QuotaExceededError(Napi::Env env) {
        return New(env, kQuotaExceededError);
    }

    Napi::Error Errors::TimeoutError(Napi::Env env) {
        return New(env, kTimeoutError);
    }

    Napi::Error Errors::InvalidNodeTypeError(Napi::Env env) {
        return New(env, kInvalidNodeTypeError);
    }

    Napi::Error Errors::DataCloneError(Napi::Env env) {
        return New(env, kDataCloneError);
    }

    Napi::Error Errors::EncodingError(Napi::Env env) {
        return New(env, kEncodingError);
    }

    Napi::Error Errors::NotReadableError(Napi::Env env) {
        return New(env, kNotReadableError);
    }

    Napi::Error Errors::UnknownError(Napi::Env env) {
        return New(env, kUnknownError);
    }

    Napi::Error Errors::ConstraintError(Napi::Env env) {
        return New(env, kConstraintError);
    }

    Napi::Error Errors::DataError(Napi::Env env) {
        return New(env, kDataError);
    }

    Napi::Error Errors::TransactionInactiveError(Napi::Env env) {
        return New(env, kTransactionInactiveError);
    }

    Napi::Error Errors::ReadOnlyError(Napi::Env env) {
        return New(env, kReadOnlyError);
    }

    Napi::Error Errors::VersionError(Napi::Env env) {
        return New(env, kVersionError);
    }

    Napi::Error Errors::OperationError(Napi::Env env) {
        return New(env, kOperationError);
    }

    Napi::Error Errors::NotAllowedError(Napi::Env env) {
        return New(env, kNotAllowedError);
    }

}}  // namespace wgpu::binding
