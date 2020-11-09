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
#error "NSRef can only be used in Objective C/C++ code."
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
class NSProtocolRef : public RefBase<T, NSProtocalRefTraits<T>> {
  public:
    using RefBase<T, NSProtocalRefTraits<T>>::RefBase;

    operator T () const {
        return this->Get();
    }
};

#endif // COMMON_NSREF_H_
