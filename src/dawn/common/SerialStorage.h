// Copyright 2018 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_COMMON_SERIALSTORAGE_H_
#define SRC_DAWN_COMMON_SERIALSTORAGE_H_

#include <cstdint>
#include <utility>

#include "dawn/common/Assert.h"
#include "partition_alloc/pointers/raw_ptr.h"

namespace dawn {

template <typename T>
struct SerialStorageTraits {};

template <typename Derived>
class SerialStorage {
  protected:
    using Serial = typename SerialStorageTraits<Derived>::Serial;
    using Value = typename SerialStorageTraits<Derived>::Value;
    using Storage = typename SerialStorageTraits<Derived>::Storage;
    using StorageIterator = typename SerialStorageTraits<Derived>::StorageIterator;
    using ConstStorageIterator = typename SerialStorageTraits<Derived>::ConstStorageIterator;

  public:
    using Iterator = StorageIterator;
    using ConstIterator = ConstStorageIterator;

    class ValueIterator : public Iterator {
      public:
        Value& operator*() const { return Iterator::operator*().second; }
    };

    class ConstValueIterator : public ConstIterator {
      public:
        const Value& operator*() const { return ConstIterator::operator*().second; }
    };

    class BeginEnd {
      public:
        BeginEnd(StorageIterator start, StorageIterator end) : mStartIt(start), mEndIt(end) {}

        const ValueIterator& begin() const { return mStartIt; }
        const ValueIterator& end() const { return mEndIt; }

      private:
        ValueIterator mStartIt;
        ValueIterator mEndIt;
    };

    class ConstBeginEnd {
      public:
        ConstBeginEnd(StorageIterator start, StorageIterator end) : mStartIt(start), mEndIt(end) {}

        const ConstValueIterator& begin() const { return mStartIt; }
        const ConstValueIterator& end() const { return mStartIt; }

      private:
        ConstValueIterator mStartIt;
        ConstValueIterator mEndIt;
    };

    Iterator begin() { return mStorage.begin(); }
    Iterator end() { return mStorage.end(); }
    ConstIterator begin() const { return mStorage.begin(); }
    ConstIterator end() const { return mStorage.end(); }

    // Derived classes may specialize constraits for elements stored
    // Ex.) SerialQueue enforces that the serial must be given in (not strictly)
    //      increasing order
    template <typename... Params>
    void Enqueue(Params&&... args, Serial serial) {
        Derived::Enqueue(std::forward<Params>(args)..., serial);
    }

    bool Empty() const;

    // The UpTo variants of Iterate and Clear affect all values associated to a serial
    // that is smaller OR EQUAL to the given serial. Iterating is done like so:
    //     for (const T& value : queue.IterateAll()) { stuff(T); }
    ConstBeginEnd IterateAll() const;
    ConstBeginEnd IterateUpTo(Serial serial) const;
    BeginEnd IterateAll();
    BeginEnd IterateUpTo(Serial serial);

    void Clear();
    void ClearUpTo(Serial serial);

    Serial FirstSerial() const;
    Serial LastSerial() const;

  protected:
    // Returns the first StorageIterator that a serial bigger than serial.
    ConstStorageIterator FindUpTo(Serial serial) const;
    StorageIterator FindUpTo(Serial serial);
    Storage mStorage;
};

// SerialStorage

template <typename Derived>
bool SerialStorage<Derived>::Empty() const {
    return mStorage.empty();
}

template <typename Derived>
typename SerialStorage<Derived>::ConstBeginEnd SerialStorage<Derived>::IterateAll() const {
    return {mStorage.begin(), mStorage.end()};
}

template <typename Derived>
typename SerialStorage<Derived>::ConstBeginEnd SerialStorage<Derived>::IterateUpTo(
    Serial serial) const {
    return {mStorage.begin(), FindUpTo(serial)};
}

template <typename Derived>
typename SerialStorage<Derived>::BeginEnd SerialStorage<Derived>::IterateAll() {
    return {mStorage.begin(), mStorage.end()};
}

template <typename Derived>
typename SerialStorage<Derived>::BeginEnd SerialStorage<Derived>::IterateUpTo(Serial serial) {
    return {mStorage.begin(), FindUpTo(serial)};
}

template <typename Derived>
void SerialStorage<Derived>::Clear() {
    mStorage.clear();
}

template <typename Derived>
void SerialStorage<Derived>::ClearUpTo(Serial serial) {
    mStorage.erase(mStorage.begin(), FindUpTo(serial));
}

template <typename Derived>
typename SerialStorage<Derived>::Serial SerialStorage<Derived>::FirstSerial() const {
    DAWN_ASSERT(!Empty());
    return mStorage.begin()->first;
}

template <typename Derived>
typename SerialStorage<Derived>::Serial SerialStorage<Derived>::LastSerial() const {
    DAWN_ASSERT(!Empty());
    return mStorage.back().first;
}

template <typename Derived>
typename SerialStorage<Derived>::ConstStorageIterator SerialStorage<Derived>::FindUpTo(
    Serial serial) const {
    return SerialStorageTraits<Derived>::FindUpTo(mStorage, serial);
}

template <typename Derived>
typename SerialStorage<Derived>::StorageIterator SerialStorage<Derived>::FindUpTo(Serial serial) {
    return SerialStorageTraits<Derived>::FindUpTo(mStorage, serial);
}

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_SERIALSTORAGE_H_
