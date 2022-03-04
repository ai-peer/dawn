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

#ifndef DAWN_COMMON_TYPEID_H_
#define DAWN_COMMON_TYPEID_H_

// namespace ityp {
//     template <typename Index, size_t N>
//     class bitset;
// }

namespace dawn {

    namespace detail {
        inline unsigned int inlTypeIDSeq = 1;
    }  // namespace detail

    using TypeID_t = unsigned int;
    template <typename T>
    inline const TypeID_t TypeID = detail::inlTypeIDSeq++;

    // template<typename Index, size_t N>
    // const TypeID_t TypeID<ityp::bitset<Index, N>> = TypeID<std::bitset<N>>;

}  // namespace dawn

#endif  // DAWN_COMMON_TYPEID_H_