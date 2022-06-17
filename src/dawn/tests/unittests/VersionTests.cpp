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

#include <string>

#include "dawn/common/Version_autogen.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace dawn {
namespace {

using ::testing::SizeIs;

#ifndef DAWN_VERSION
TEST(VersionTests, GitCommitHashLength) {
    // When an explicit Dawn version is not set, version should be a git hash which are length 40.
    EXPECT_THAT(kDawnVersion, SizeIs(40));
}
#else
TEST(VersionTests, ExplicitDawnVersion) {
    EXPECT_EQ(kDawnVersion, DAWN_VERSION);
}
#endif

}  // namespace
}  // namespace dawn
