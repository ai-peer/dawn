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

#ifndef WORKAROUNDS_H_
#define WORKAROUNDS_H_

#include <map>

namespace dawn_native {

    const char* const kEmulateStoreAndMSAAResolve = "emulate_store_and_msaa_resolve";

    using WorkaroundsController = std::map<const char*, bool>;

}  // namespace dawn_native

#endif  // DAWNNATIVE_WORKAROUNDS_H_
