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

#ifndef DAWN_COMMON_INMEMORYBLOBSTORE_H_
#define DAWN_COMMON_INMEMORYBLOBSTORE_H_

#include "dawn/common/Blob.h"
#include "dawn/common/Serialize.h"

#include <optional>
#include <utility>
#include <variant>

namespace dawn {

    class InMemoryBlobStore {
      public:
        using TemporaryKey = std::string_view;
        using StoredKey = std::string;
        using Key = std::variant<TemporaryKey, StoredKey>;
        using Value = Blob;

        // TODO: Disallow nesting.
        template <typename... Args>
        TemporaryKey MakeTemporaryKey(const Args&... args) {
            struct TempKeySink {
                std::vector<uninitialized_char>* tempKeyData;
                size_t size;

                char* Alloc(size_t allocSize) {
                    size = allocSize;
                    tempKeyData->resize(allocSize);
                    return reinterpret_cast<char*>(tempKeyData->data());
                }

            } sink = {&mTempKeyData, 0};

            Serialize(&sink, args...);
            return TemporaryKey(reinterpret_cast<char*>(this->mTempKeyData.data()), sink.size);
        }

        static StoredKey IntoStorageKey(TemporaryKey tempKey) {
            return StoredKey(tempKey);
        }

        template <typename R>
        std::optional<R> Load(const Key& key) {
            auto it = mData.find(key);
            if (it == mData.end()) {
                return {};
            }
            return R(it->second);
        }

        template <typename V>
        void Store(const StoredKey& key, const V& value) {
            auto it = mData.find(key);
            if (it == mData.end()) {
                mData.emplace(key, Blob(value));
            } else {
                it->second = Blob(value);
            }
        }

        template <typename V>
        void Store(StoredKey&& key, const V& value) {
            auto it = mData.find(key);
            if (it == mData.end()) {
                mData.emplace(std::move(key), Blob(value));
            } else {
                it->second = Blob(value);
            }
        }

      private:
        struct uninitialized_char {
            char val;
            uninitialized_char() {
            }  // prevent zero-initialization of val
            operator char() const {
                return val;
            }  // make implicitly convertible to char
        };

        struct KeyHash {
            size_t operator()(const Key& k) const {
                std::string_view view = std::holds_alternative<std::string>(k)
                                            ? std::string_view(std::get<std::string>(k))
                                            : std::get<std::string_view>(k);
                return std::hash<std::string_view>{}(view);
            }
        };

        struct KeyEqual {
            bool operator()(const Key& a, const Key& b) const {
                std::string_view viewA = std::holds_alternative<std::string>(a)
                                             ? std::string_view(std::get<std::string>(a))
                                             : std::get<std::string_view>(a);

                std::string_view viewB = std::holds_alternative<std::string>(b)
                                             ? std::string_view(std::get<std::string>(b))
                                             : std::get<std::string_view>(b);

                return viewA.compare(viewB) == 0;
            }
        };

        std::unordered_map<Key, Value, KeyHash, KeyEqual> mData;
        std::vector<uninitialized_char> mTempKeyData;
    };

}  // namespace dawn

#endif  // DAWN_COMMON_INMEMORYBLOBSTORE_H_