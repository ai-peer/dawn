#!/usr/bin/env python3
# Copyright 2022 The Dawn Authors
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

import subprocess, sys

from generator_lib import Generator, run_generator, FileRender


def get_gitHash():
    git = 'git.bat' if sys.platform == 'win32' else 'git'
    result = subprocess.run([git, 'rev-parse', 'HEAD'], stdout=subprocess.PIPE)
    if result.returncode == 0:
        return result.stdout.decode('utf-8').strip()
    return ''


def compute_params():
    return {
        'get_gitHash': get_gitHash,
    }


class DawnCommonGenerator(Generator):
    def get_description(self):
        return 'Generates common Dawn code that does not need the json file.'

    def get_file_renders(self, args):
        params = compute_params()

        return [
            FileRender('dawn/common/Version.h',
                       'src/dawn/common/Version_autogen.h', [params]),
        ]


if __name__ == '__main__':
    sys.exit(run_generator(DawnCommonGenerator()))
