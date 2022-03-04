// Copyright 2022 The Dawn Authors
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

#ifndef DAWN_COMMON_MEMOIZE_H_
#define DAWN_COMMON_MEMOIZE_H_

#include "dawn/common/Result.h"
#include "dawn/common/TypeID.h"

#include <iostream>
#include <optional>

namespace dawn {

    namespace detail {

        template <typename MemoStorage, typename R, typename... Args>
        auto MemoizeImpl(MemoStorage* storage, TypeID_t factoryID, R (*FactoryFn)(Args...)) {
            return [storage, factoryID, FactoryFn](Args... args) -> R {
                auto tempKey = storage->MakeTemporaryKey(factoryID, args...);
                auto cachedResult = storage->template Load<R>(tempKey);
                if (cachedResult) {
                    return std::move(*cachedResult);
                }

                auto storedKey = storage->IntoStorageKey(std::move(tempKey));
                auto result = FactoryFn(std::forward<Args>(args)...);
                storage->Store(std::move(storedKey), result);
                return result;
            };
        }

        template <typename MemoStorage, typename S, typename E, typename... Args>
        auto MemoizeImpl(MemoStorage* storage,
                         TypeID_t factoryID,
                         Result<S, E> (*FactoryFn)(Args...)) {
            return [storage, factoryID, FactoryFn](Args... args) -> Result<S, E> {
                auto tempKey = storage->MakeTemporaryKey(factoryID, args...);
                auto cachedResult = storage->template Load<S>(tempKey);
                if (cachedResult) {
                    return {std::move(*cachedResult)};
                }

                auto storedKey = storage->IntoStorageKey(std::move(tempKey));
                Result<S, E> result = FactoryFn(std::forward<Args>(args)...);
                if (result.IsError()) {
                    return result;
                }
                auto successValue = result.AcquireSuccess();
                storage->Store(std::move(storedKey), successValue);
                return successValue;
            };
        }

    }  // namespace detail

    template <typename Factory, typename MemoStorage>
    auto Memoize(MemoStorage* storage) {
        return detail::MemoizeImpl(storage, TypeID<Factory>, Factory::Create);
    }

    template <typename T>
    struct Unkeyed {
        T value;

        T& operator*() {
            return value;
        }
    };

    template <typename T>
    Unkeyed<T> PassUnkeyed(T value) {
        return Unkeyed<T>{std::move(value)};
    }

}  // namespace dawn

#endif  // DAWN_COMMON_MEMOIZE_H_
