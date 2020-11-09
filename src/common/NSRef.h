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

#ifndef COMMON_NSREF_H_
#define COMMON_NSREF_H_

#include "common/RefBase.h"

#import <Foundation/NSObject.h>

#if !defined(__OBJC__)
#    error "NSRef can only be used in Objective C/C++ code."
#endif

template <typename T>
struct NSProtocalRefTraits {
    using PointedType = void;
    static constexpr T kNullValue = nil;
    static void Reference(T value) {
        [value retain];
    }
    static void Release(T value) {
        [value release];
    }
};

template <typename T>
class NSRef : public RefBase<T*, NSProtocalRefTraits<T*>> {
  public:
    using RefBase<T*, NSProtocalRefTraits<T*>>::RefBase;

    const T* operator*() const {
        return this->Get();
    }

    T* operator*() {
        return this->Get();
    }
};

template <typename T>
NSRef<T> AcquireNSRef(T* pointee) {
    NSRef<T> ref;
    ref.Acquire(pointee);
    return ref;
}

// This is a RefBase<> for an Objective C protocal (hence the P). Objective C protocals must always
// be referenced with id<ProtocolName> and not just ProtocolName* so they cannot use NSRef<>
// itself. That's what the P in NSPRef stands for: Protocol.
template <typename T>
class NSPRef : public RefBase<T, NSProtocalRefTraits<T>> {
  public:
    using RefBase<T, NSProtocalRefTraits<T>>::RefBase;

    const T operator*() const {
        return this->Get();
    }

    T operator*() {
        return this->Get();
    }
};

template <typename T>
NSPRef<T> AcquireNSPRef(T pointee) {
    NSPRef<T> ref;
    ref.Acquire(pointee);
    return ref;
}

#endif  // COMMON_NSREF_H_
