// Copyright 2017 The Dawn & Tint Authors
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

#ifndef SRC_DAWN_COMMON_REFCOUNTED_H_
#define SRC_DAWN_COMMON_REFCOUNTED_H_

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace dawn {

namespace detail {
class WeakRefData;
}  // namespace detail

template <bool RefCountStartFromZero>
class RefCountBase {
  public:
    // Create a refcount with a payload. The refcount starts initially at one.
    explicit RefCountBase(uint64_t payload = 0);

    uint64_t GetValueForTesting() const;
    uint64_t GetPayload() const;

    // Add a reference. Return true if the ref count is 0 before increment.
    bool Increment();
    // If the RefCount is created with RefCountStartFromZero is true, TryIncrement() always returns
    // true and increase the refcount. Tries to add a reference. Returns false if the ref count is
    // already at 0. This is used when operating on a raw pointer to a RefCounted instead of a valid
    // Ref that may be soon deleted.
    bool TryIncrement();

    // Remove a reference. Returns true if this was the last reference.
    bool Decrement();

  private:
    std::atomic<uint64_t> mRefCount;
};

// Specialize template method declare for RefCountStartFromZero = false
extern template RefCountBase<false>::RefCountBase(uint64_t payload);
extern template uint64_t RefCountBase<false>::GetValueForTesting() const;
extern template uint64_t RefCountBase<false>::GetPayload() const;
extern template bool RefCountBase<false>::Increment();
extern template bool RefCountBase<false>::TryIncrement();
extern template bool RefCountBase<false>::Decrement();

// Specialize template declare for RefCountStartFromZero = true
extern template RefCountBase<true>::RefCountBase(uint64_t payload);
extern template uint64_t RefCountBase<true>::GetValueForTesting() const;
extern template uint64_t RefCountBase<true>::GetPayload() const;
extern template bool RefCountBase<true>::Increment();
extern template bool RefCountBase<true>::TryIncrement();
extern template bool RefCountBase<true>::Decrement();

using RefCount = RefCountBase</*RefCountStartFromZero=*/false>;

class RefCounted {
  public:
    explicit RefCounted(uint64_t payload = 0);

    uint64_t GetRefCountForTesting() const;
    uint64_t GetRefCountPayload() const;

    void Reference();
    // Release() is called by internal code, so it's assumed that there is already a thread
    // synchronization in place for destruction.
    void Release();

    void APIReference() { Reference(); }
    // APIRelease() can be called without any synchronization guarantees so we need to use a Release
    // method that will call LockAndDeleteThis() on destruction.
    void APIRelease() { ReleaseAndLockBeforeDestroy(); }

  protected:
    // Friend class is needed to access the RefCount to TryIncrement.
    friend class detail::WeakRefData;

    virtual ~RefCounted();

    void ReleaseAndLockBeforeDestroy();

    // A Derived class may override these if they require a custom deleter.
    virtual void DeleteThis();
    // This calls DeleteThis() by default.
    virtual void LockAndDeleteThis();

    RefCount mRefCount;
};

}  // namespace dawn

#endif  // SRC_DAWN_COMMON_REFCOUNTED_H_
