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

#include <array>
#include <unordered_map>

namespace dawn_native {

    class ErrorInjector {
      public:
        ErrorInjector();
        ~ErrorInjector();

        inline static ErrorInjector* Get() {
            return gInjector;
        }

        static void Set(ErrorInjector* injector);

        template <typename ErrorType, typename... ErrorTypes>
        auto ShouldInjectError(const char* file,
                               const char* func,
                               int line,
                               ErrorType errorTypeIn,
                               ErrorTypes... errorTypesIn) {
            for (ErrorType type :
                 std::array<ErrorType, 1 + sizeof...(errorTypesIn)>{errorTypeIn, errorTypesIn...}) {
                if (ShouldInjectErrorImpl(file, func, line)) {
                    return std::make_pair(true, type);
                }
            }

            return std::make_pair(false, ErrorType{});
        }

        bool ShouldInjectErrorImpl(const char* file, const char* func, int line);

        void InjectErrorAt(size_t callsite, uint64_t index);
        std::unordered_map<size_t, uint64_t> AcquireCallLog();
        void Clear();

      private:
        static ErrorInjector* gInjector;

        std::unordered_map<size_t, uint64_t> mCallCounts;
        size_t mInjectedCallsiteFailure = 0;
        uint64_t mInjectedCallsiteFailureIndex = 0;
        bool mHasPendingInjectedError = false;
    };

}  // namespace dawn_native

#if defined(DAWN_ENABLE_ERROR_INJECTION)

#    define INJECT_ERROR_OR_RUN_IMPL(stmt, ...)                                              \
        [&](const char* func) -> decltype(stmt) {                                            \
            ::dawn_native::ErrorInjector* injector = ::dawn_native::ErrorInjector::Get();    \
            if (DAWN_UNLIKELY(injector != nullptr)) {                                        \
                /* Only used for testing and fuzzing, so it's okay if this is deoptimized */ \
                auto injectedError =                                                         \
                    injector->ShouldInjectError(__FILE__, func, __LINE__, ##__VA_ARGS__);    \
                if (injectedError.first) {                                                   \
                    return injectedError.second;                                             \
                }                                                                            \
            }                                                                                \
            return (stmt);                                                                   \
        }(__func__)

#else

#    define INJECT_ERROR_OR_RUN_IMPL(stmt, ...) stmt

#endif

#endif  // DAWNNATIVE_ERRORINJECTOR_H_
