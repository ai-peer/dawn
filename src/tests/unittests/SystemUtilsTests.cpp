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

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "common/Assert.h"
#include "common/SystemUtils.h"

namespace {

    // Matches against the return value of |GetEnvironmentVar|. Checks that the variable
    // value is |s|, and whether or not the variable was |empty|.
    MATCHER_P2(EnvVarEq, s, empty, "") {
        return ExplainMatchResult(::testing::StrEq(s), arg.first, result_listener) &&
               empty == arg.second;
    }

    // Matches against the return value of |GetEnvironmentVar|. Checks that the variable
    // value is |s|, assuming that |s| is non-empty.
    MATCHER_P(EnvVarEq, s, "") {
        ASSERT(s != nullptr);
        return ExplainMatchResult(::testing::StrEq(s), arg.first, result_listener) && arg.second;
    }

}  // namespace

// Tests for GetEnvironmentVar
TEST(SystemUtilsTests, GetEnvironmentVar) {
    // Test nonexistent environment variable
    ASSERT_THAT(GetEnvironmentVar("NonexistentEnvironmentVar"), EnvVarEq("", false));
}

// Tests for SetEnvironmentVar
TEST(SystemUtilsTests, SetEnvironmentVar) {
    // Test new environment variable
    ASSERT_TRUE(SetEnvironmentVar("EnvironmentVarForTest", "NewEnvironmentVarValue"));
    ASSERT_THAT(GetEnvironmentVar("EnvironmentVarForTest"), EnvVarEq("NewEnvironmentVarValue"));
    // Test override environment variable
    ASSERT_TRUE(SetEnvironmentVar("EnvironmentVarForTest", "OverrideEnvironmentVarValue"));
    ASSERT_THAT(GetEnvironmentVar("EnvironmentVarForTest"),
                EnvVarEq("OverrideEnvironmentVarValue"));
}

// Tests for GetExecutableDirectory
TEST(SystemUtilsTests, GetExecutableDirectory) {
    // Test returned value is non-empty string
    ASSERT_NE(GetExecutableDirectory(), "");
    // Test last charecter in path
    ASSERT_EQ(GetExecutableDirectory().back(), *GetPathSeparator());
}

// Tests for ScopedEnvironmentVar
TEST(SystemUtilsTests, ScopedEnvironmentVar) {
    SetEnvironmentVar("ScopedEnvironmentVarForTest", "original");

    // Test empty environment variable doesn't crash
    { ScopedEnvironmentVar var; }

    // Test setting empty environment variable
    {
        ScopedEnvironmentVar var;
        var.Set("ScopedEnvironmentVarForTest", "NewEnvironmentVarValue");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"),
                    EnvVarEq("NewEnvironmentVarValue"));
    }
    ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("original"));

    // Test that the environment variable can be set, and it is unset at the end of the scope.
    {
        ScopedEnvironmentVar var("ScopedEnvironmentVarForTest", "NewEnvironmentVarValue");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"),
                    EnvVarEq("NewEnvironmentVarValue"));
    }
    ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("original"));

    // Test nested scopes
    {
        ScopedEnvironmentVar outer("ScopedEnvironmentVarForTest", "outer");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("outer"));
        {
            ScopedEnvironmentVar inner("ScopedEnvironmentVarForTest", "inner");
            ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("inner"));
        }
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("outer"));
    }
    ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("original"));

    // Test redundantly setting scoped variables
    {
        ScopedEnvironmentVar var1("ScopedEnvironmentVarForTest", "var1");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("var1"));

        ScopedEnvironmentVar var2("ScopedEnvironmentVarForTest", "var2");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("var2"));
    }
    ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("original"));
}

// Test that restoring a scoped environment variable to the empty string.
TEST(SystemUtilsTests, ScopedEnvironmentVarRestoresEmptyString) {
    ScopedEnvironmentVar empty("ScopedEnvironmentVarForTest", "");
    {
        ScopedEnvironmentVar var1("ScopedEnvironmentVarForTest", "var1");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("var1"));
    }
    ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("", true));
}

// Test that restoring a scoped environment variable to not set (distuishable from empty string)
// works.
TEST(SystemUtilsTests, ScopedEnvironmentVarRestoresNotSet) {
    ScopedEnvironmentVar null("ScopedEnvironmentVarForTest", nullptr);
    {
        ScopedEnvironmentVar var1("ScopedEnvironmentVarForTest", "var1");
        ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("var1"));
    }
    ASSERT_THAT(GetEnvironmentVar("ScopedEnvironmentVarForTest"), EnvVarEq("", false));
}
