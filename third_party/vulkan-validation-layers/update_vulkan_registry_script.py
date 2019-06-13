#!/usr/bin/env python
# Copyright 2019 The Dawn Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This script update Vulkan-Headers/registry/generator.py to run with python2
# and python3 for pathlib importing
import os
import sys

def find(str, lines):
    for line in lines:
        if str in line:
            return True
    return False

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: %s STAMP FILE" % os.path.basename(__file__))
        sys.exit(1)

    with open(sys.argv[2], "r") as f:
        lines = f.readlines()

    if not find("from pathlib2 import Path", lines):
        with open(sys.argv[2], "w") as fw:
            pathlib = "from pathlib import Path"
            for line in lines:
                if pathlib in line:
                    pathlib2 = "try:\n    from pathlib import Path\nexcept:\n    os.system(\"pip install pathlib2\")\n    from pathlib2 import Path\n"
                    line = line.replace(pathlib, pathlib2)
                fw.write(line)

        f.close()
        fw.close()

    # touch a dummy file to keep a timestamp
    with open(sys.argv[1], "w") as fs:
        fs.write("blah")
        fs.close()
