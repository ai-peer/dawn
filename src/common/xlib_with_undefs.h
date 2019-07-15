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

#ifndef COMMON_XLIB_WITH_UNDEFS_H_
#define COMMON_XLIB_WITH_UNDEFS_H_

#include "common/Compiler.h"

#if !defined(DAWN_PLATFORM_LINUX)
#    error "xlib_with_undefs.h included on non-Linux"
#endif

// <X11.Xlib.h> will define some macros like Success as literal constants
// which conflict with other Dawn sources that use the same names as
// enum values. Ensure they are undefined just after the include!
#include <X11/Xlib.h>

#undef Success
#undef None
#undef Always

#endif  // COMMON_XLIB_WITH_UNDEFS_H_
