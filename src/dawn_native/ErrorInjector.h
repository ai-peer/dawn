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

#ifndef DAWNNATIVE_ERRORINJECTOR_H_
#define DAWNNATIVE_ERRORINJECTOR_H_

#include <stdint.h>

namespace dawn_native {

    class ErrorInjector {
      public:
        static bool IsEnabled() {
            return sIsEnabled;
        }

        static void Enable();
        static void Disable();

        template <typename ErrorType>
        struct InjectedErrorResult {
            ErrorType error;
            bool injected;
        };

        template <typename ErrorType, typename... ErrorTypes>
        static InjectedErrorResult<ErrorType> ShouldInjectError(ErrorType errorType,
                                                                ErrorTypes... errorTypes) {
            if (ShouldInjectErrorImpl()) {
                return InjectedErrorResult<ErrorType>{errorType, true};
            }
            return ShouldInjectError(errorTypes...);
        }

        template <typename ErrorType>
        static InjectedErrorResult<ErrorType> ShouldInjectError(ErrorType errorType) {
            return InjectedErrorResult<ErrorType>{errorType, ShouldInjectErrorImpl()};
        }

        static bool ShouldInjectErrorImpl();

        static void InjectErrorAt(uint64_t index);
        static uint64_t AcquireCallCounts();
        static void Clear();

      private:
        static bool sIsEnabled;
    };

}  // namespace dawn_native

#if defined(DAWN_ENABLE_ERROR_INJECTION)

#    define INJECT_ERROR_OR_RUN(stmt, ...)                                                         \
        [&]() -> decltype(stmt) {                                                                  \
            if (DAWN_UNLIKELY(::dawn_native::ErrorInjector::IsEnabled())) {                        \
                /* Only used for testing and fuzzing, so it's okay if this is deoptimized */       \
                auto injectedError = ::dawn_native::ErrorInjector::ShouldInjectError(__VA_ARGS__); \
                if (injectedError.injected) {                                                      \
                    return injectedError.error;                                                    \
                }                                                                                  \
            }                                                                                      \
            return (stmt);                                                                         \
        }()

#else

#    define INJECT_ERROR_OR_RUN(stmt, ...) stmt

#endif

#endif  // DAWNNATIVE_ERRORINJECTOR_H_
