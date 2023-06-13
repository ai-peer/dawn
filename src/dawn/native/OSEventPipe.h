// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_DAWN_SRC_DAWN_NATIVE_OSEVENTPIPE_H_
#define THIRD_PARTY_DAWN_SRC_DAWN_NATIVE_OSEVENTPIPE_H_

#include <utility>

#include "dawn/native/Error.h"
#include "dawn/native/OSEvent.h"

namespace dawn::native {

class OSEventPipe final {
  public:
    static ResultOrError<std::pair<OSEventPipe, OSEventReceiver>> CreateEventPipe();

    OSEventPipe() = default;
    explicit OSEventPipe(OSEventPipe&&);
    OSEventPipe& operator=(OSEventPipe&&);
    ~OSEventPipe();

    MaybeError Signal();

  private:
    OSEventPrimitive mPrimitive;
};

}  // namespace dawn::native

#endif  // THIRD_PARTY_DAWN_SRC_DAWN_NATIVE_OSEVENTPIPE_H_
