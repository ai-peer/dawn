#!/usr/bin/python2
#
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

"""Generates a seed corpus for fuzzing based on dumping wire traces
from running Dawn tests"""

import argparse
import subprocess
import sys
import os
import shutil

base_path = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


binary_name = 'dawn_end2end_tests'
if sys.platform == 'win32':
    binary_name += '.exe'

def main():
    # parser = argparse.ArgumentParser(description=__doc__)
    # parser.add_argument('--dir', required=True, help='The build directory ex.) out/Release')
    # args = parser.parse_args()

    # testcases_path = os.path.join(base_path, 'src/fuzzers/testcases/')
    # if os.path.exists(testcases_path):
    #     shutil.rmtree(testcases_path)

    # os.mkdir(testcases_path)

    # binary_path = os.path.join(base_path, args.dir, binary_name)
    # subprocess.call([binary_path, '-w', '--wire-trace-dir=' + testcases_path, '--gtest_filter=*/Vulkan'])

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test', type=str, required=True, help='The test binary')
    parser.add_argument('--fuzzer', type=str, required=True, help='The fuzzer binary')
    parser.add_argument('--intermediate_output', type=str, required=True,
                        help='The intermediate output directory for testcases before minimization')
    parser.add_argument('--output', type=str, required=True, help='The output directory')
    parser.add_argument('--test_args', type=str, required=True, help='The test arguments')
    parser.add_argument('--stamp', type=str, help='A stamp written once this script completes')
    args = parser.parse_args()

    if os.path.exists(args.intermediate_output):
        shutil.rmtree(args.intermediate_output)
    os.mkdir(args.intermediate_output)

    if os.path.exists(args.output):
        shutil.rmtree(args.output)
    os.mkdir(args.output)

    subprocess.call([args.test, '--wire-trace-dir=' + args.intermediate_output] + args.test_args.split(' '))
    subprocess.call([args.fuzzer, '-merge=1', args.output, args.intermediate_output])

    # Finished! Write the stamp file so ninja knows to not run this again.
    with open(args.stamp, 'w') as f:
        f.write('')

if __name__ == '__main__':
    sys.exit(main())
