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

binary_suffix = ''
if sys.platform == 'win32':
    binary_suffix += '.exe'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fuzzer', type=str, required=True, help='The fuzzer target')
    parser.add_argument('--output', type=str, required=True,
                        help='The output directory for testcases before minimization')
    parser.add_argument('--minimized_output', type=str, required=True,
                        help='The output directory for minimuzed testcases')
    parser.add_argument('--test', type=str, required=True, help='The test target')
    parser.add_argument('--test_args', type=str, required=True, help='The test arguments')
    args = parser.parse_args()

    # Build the fuzzer and test
    build_command = ['autoninja', '-C', 'out/Release', args.fuzzer, args.test]
    print ' '.join(build_command)
    subprocess.call(build_command)

    fuzzer_binary = os.path.normpath(os.path.join('out/Release', args.fuzzer + binary_suffix))
    test_binary = os.path.normpath(os.path.join('out/Release', args.test + binary_suffix))

    # Run the test
    test_command = [test_binary] + args.test_args.split(' ')
    print ' '.join(test_command)
    subprocess.call(test_command)

    # Run the fuzzer to minimize the corpus
    minimize_command = [fuzzer_binary, '-merge=1', args.minimized_output, args.output]
    print ' '.join(minimize_command)
    subprocess.call(minimize_command)

if __name__ == '__main__':
    sys.exit(main())
