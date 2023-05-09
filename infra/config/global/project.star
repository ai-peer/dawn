#
# Copyright 2023 The Dawn Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

def _milestone_details(*, project, chromium_ref):
  """Define the details for an active milestone.

  Args:
    * project - The name of the LUCI project that is configured for the
      milestone.
    * chromium_ref - The ref in the Chromium git repository that contains the
      code for the milestone.
  """
  branch_number = chromium_ref.split('/')[-1]
  return struct(
      project = project,
      chromium_ref = chromium_ref,
      dawn_ref = "chromium/" + branch_number,
  )

ACTIVE_MILESTONES = {
    m["name"]: _milestone_details(
        project = m["project"], chromium_ref = m["ref"])
        for m in json.decode(io.read_file("./milestones.json")).values()
}
