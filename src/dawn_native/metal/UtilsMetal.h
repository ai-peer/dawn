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

#ifndef DAWNNATIVE_METAL_UTILSMetal_H_
#define DAWNNATIVE_METAL_UTILSMetal_H_

namespace dawn_native { namespace metal {

    MTLCompareFunction MetalCompareFunction(dawn::CompareFunction compareFunction) {
        switch (compareFunction) {
            case dawn::CompareFunction::Never:
                return MTLCompareFunctionNever;
            case dawn::CompareFunction::Less:
                return MTLCompareFunctionLess;
            case dawn::CompareFunction::LessEqual:
                return MTLCompareFunctionLessEqual;
            case dawn::CompareFunction::Greater:
                return MTLCompareFunctionGreater;
            case dawn::CompareFunction::GreaterEqual:
                return MTLCompareFunctionGreaterEqual;
            case dawn::CompareFunction::NotEqual:
                return MTLCompareFunctionNotEqual;
            case dawn::CompareFunction::Equal:
                return MTLCompareFunctionEqual;
            case dawn::CompareFunction::Always:
                return MTLCompareFunctionAlways;
        }
    }
}}  // namespace dawn_native::metal

#endif  // DAWNNATIVE_METAL_UTILSMETAL_H_
