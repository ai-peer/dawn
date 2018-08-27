# Copyright (c) 2018 The Dawn Authors. All rights reserved.#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import platform
import subprocess

def _DoClangFormat(input_api, output_api):
    # Our binary clang-format is a linux binary compiled for x64
    if platform.system() != 'Linux' and platform.architecture()[1] != '64bit':
        return []

    # We need to know which commit to diff against. It doesn't seem to be exposed anywhere
    # except in that private member of presubmit_support.Change. This is likely to break
    # but hopefully we have an updated clang-format in CPID/GS before it does.
    upstream_commit = input_api.change._upstream
    if upstream_commit == None:
        return []

    rc = subprocess.call(["scripts/lint_clang_format.sh",
                          "third_party/clang-format/clang-format",
                          upstream_commit]);

    warnings = []
    if rc != 0:
      warnings.append(output_api.PresubmitPromptWarning(
          'Formatting errors shown above, please fix them.'))

    return warnings

def _DoCommonChecks(input_api, output_api):
    results = []
    results.extend(input_api.canned_checks.CheckChangedLUCIConfigs(input_api, output_api))
    results.extend(input_api.canned_checks.CheckGNFormatted(input_api, output_api))
    results.extend(_DoClangFormat(input_api, output_api))
    return results

def CheckChangeOnUpload(input_api, output_api):
    return _DoCommonChecks(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
    return _DoCommonChecks(input_api, output_api)
