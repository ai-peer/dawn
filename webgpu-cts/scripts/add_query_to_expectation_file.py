#!/usr/bin/env python3
#
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

"""Script for easily adding expectations to expectations.txt

Converts a single WebGPU CTS query into one or more individual expectations and
appends them to the end of the file.
"""

import argparse
import os
import subprocess

import dir_paths

LIST_SCRIPT_PATH = os.path.join(dir_paths.webgpu_cts_scripts_dir, 'list.py')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-b', '--bug',
                        help='The bug link to associate with the expectations')
    parser.add_argument('-t', '--tag', action='append', default=[],
                        help=('A tag to restrict the expectation to. Can be '
                              'specified multiple times.'))
    parser.add_argument('-r', '--result', action='append', default=[], required=True)