// Copyright 2020 The Dawn Authors
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

#ifndef COMMON_ITYP_ARRAY_VEC_H_
#define COMMON_ITYP_ARRAY_VEC_H_

#include "common/Assert.h"
#include "common/StackContainer.h"
#include "common/UnderlyingType.h"

namespace ityp {

    template <typename Index, typename Value, size_t StaticCapacity = 1>
    class array_vec : private StackVector<Value, StaticCapacity> {
        using I = UnderlyingType<Index>;
        using Base = StackVector<Value, StaticCapacity>;

      public:
        array_vec() : Base() {
        }
        array_vec(Index size) : Base() {
            this->container().resize(static_cast<I>(size));
        }

        Value& operator[](Index i) {
            return Base::operator[](static_cast<I>(i));
        }

        constexpr const Value& operator[](Index i) const {
            return Base::operator[](static_cast<I>(i));
        }

        void resize(Index size) {
            this->container().resize(static_cast<I>(size));
        }

        void reserve(Index size) {
            this->container().reserve(static_cast<I>(size));
        }

        Value* data() {
            return this->container().data();
        }

        const Value* data() const {
            return this->container().data();
        }

        Value* begin() noexcept {
            return this->container().begin();
        }

        const Value* begin() const noexcept {
            return this->container().begin();
        }

        Value* end() noexcept {
            return this->container().end();
        }

        const Value* end() const noexcept {
            return this->container().end();
        }

        Value& front() {
            return this->container().front();
        }

        const Value& front() const {
            return this->container().front();
        }

        Value& back() {
            return this->container().back();
        }

        const Value& back() const {
            return this->container().back();
        }

        Index size() const {
            return Index(static_cast<I>(this->container().size()));
        }
    };

    /*template <typename Index, typename Value, size_t StaticSize = 0>
    class array_vec {
        using I = UnderlyingType<Index>;

      public:
        constexpr array_vec()
            : mSize(0), mType(Type::Small), mArray{}, mData(mArray.data()) {
        }

        constexpr array_vec(Index size)
            : mSize(size), mType(static_cast<I>(size) <= StaticSize ? Type::Small : Type::Large) {
            switch (mType) {
                case Type::Small:
                    mArray = {};
                    mData = mArray.data();
                    break;
                case Type::Large:
                    mVector = std::vector<Value>(static_cast<I>(size));
                    mData = mVector.data();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }

        array_vec(array_vec&& other) : mSize(other.mSize), mType(other.mType) {
            switch (mType) {
                case Type::Small:
                    mArray = std::move(other.mArray);
                    mData = mArray.data();
                    break;
                case Type::Large:
                    mVector = std::move(other.mVector);
                    mData = mVector.data();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
        }

        array_vec& operator=(array_vec&& other) {
            this->~array_vec();

            mSize = other.mSize;
            mType = other.mType;
            switch (mType) {
                case Type::Small:
                    mArray = std::move(other.mArray);
                    mData = mArray.data();
                    break;
                case Type::Large:
                    mVector = std::move(other.mVector);
                    mData = mVector.data();
                    break;
                default:
                    UNREACHABLE();
                    break;
            }
            return *this;
        }

        void resize(Index size, Value init = {}) {
            switch (mType) {
                case Type::Small:
                    if (static_cast<I>(size) <= StaticSize) {
                        // Both small. No change.
                        break;
                    } else {
                        // Convert std::array to std::vector

                        // First reserve, not resize because we don't want to initialize the
                        // contents.
                        mVector.reserve(static_cast<I>(size));
                        mVector.insert(mVector.end(), mArray.begin(), mArray.end());

                        // Then, resize to initialize the remaining elements.
                        mVector.resize(static_cast<I>(size), init);
                        mData = mVector.data();
                        break;
                    }
                case Type::Large:
                    if (static_cast<I>(size) <= StaticSize) {
                        auto first = mVector.cbegin();
                        auto last = first + static_cast<I>(size);

                        mData = mArray.data();
                        Value* out = mData;
                        while (first != last) {
                            *out++ = *first++;
                        }
                        mVector.clear();
                        break;
                    } else {
                        // Both large. Call the std::vector resize function.
                        mVector.resize(static_cast<I>(size), init);
                        mData = mVector.data();
                        break;
                    }
                default:
                    UNREACHABLE();
                    break;
            }
            mSize = size;
        }

        void ensure(Index size) {
            if (size <= mSize) {
                mSize = size;
                return;
            }
            resize(size);
        }

        void ensure(Index size, Value init) {
            if (size <= mSize) {
                mSize = size;
                return;
            }
            resize(size, init);
        }

        Value& operator[](Index i) {
            return mData[static_cast<I>(i)];
        }

        constexpr const Value& operator[](Index i) const {
            return mData[static_cast<I>(i)];
        }

        Value* data() {
            return mData;
        }

        const Value* data() const {
            return mData;
        }

        Value* begin() noexcept {
            return mData;
        }

        const Value* begin() const noexcept {
            return mData;
        }

        Value* end() noexcept {
            return mData + static_cast<I>(size());
        }

        const Value* end() const noexcept {
            return mData + static_cast<I>(size());
        }

        Value& front() {
            ASSERT(mData != nullptr);
            ASSERT(static_cast<I>(mSize) >= 0);
            return *mData;
        }

        const Value& front() const {
            ASSERT(mData != nullptr);
            ASSERT(static_cast<I>(mSize) >= 0);
            return *mData;
        }

        Value& back() {
            ASSERT(mData != nullptr);
            ASSERT(static_cast<I>(mSize) >= 0);
            return *(mData + static_cast<I>(mSize) - 1);
        }

        const Value& back() const {
            ASSERT(mData != nullptr);
            ASSERT(static_cast<I>(mSize) >= 0);
            return *(mData + static_cast<I>(mSize) - 1);
        }

        Index size() const {
            return mSize;
        }

      private:
        Index mSize;

        enum class Type {
            Small,
            Large,
        } mType;

        std::array<Value, StaticSize> mArray;
        std::vector<Value> mVector;

        Value* mData;
    };*/

}  // namespace ityp

#endif  // COMMON_ITYP_ARRAY_VEC_H_
