#!/bin/bash

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

# PREREQUISITES
# -----------------------------------------------------------------------------
# 1. Make sure gcloud is installed:
#    https://g3doc.corp.google.com/cloud/sdk/g3doc/index.md?cl=head
#
#   sudo apt install -y google-cloud-sdk
#
# 2. Login with @google.com credentials
#
#   gcloud auth login
#
# 3. This script must be run from a Chromium checkout with use_libfuzzer=true

fuzzer_name=$1
test_name=$2
base_dir=$(dirname "$0")

if [ -z "$1" ] || [ -z "$2" ]; then
cat << EOF

Usage:
  $0 <fuzzer_name> <test_name>

Example:
  $0 dawn_wire_server_and_frontend_fuzzer dawn_end2end_tests

EOF
    exit 1
fi

testcase_dir=/tmp/testcases/${fuzzer_name}/
minimized_testcase_dir=/tmp/testcases/${fuzzer_name}_minimized/

# Exit if anything fails
set -e

# Make a directory for temporarily storing testcases
mkdir -p $testcase_dir

# Make an empty directory for temporarily storing minimized testcases
rm -rf $minimized_testcase_dir
mkdir -p $minimized_testcase_dir

# Download the existing corpus. First argument is src, second is dst.
gsutil -m rsync gs://clusterfuzz-corpus/libfuzzer/${fuzzer_name}/ $testcase_dir

# Run the test binary, tracing commands, and then run the fuzzer to minimize the corpus.
python $base_dir/generate_seed_corpus_from_tests.py \
    --fuzzer $fuzzer_name \
    --test $test_name \
    --output $testcase_dir \
    --minimized_output $minimized_testcase_dir \
    --test_args "--use-wire --wire-trace-dir=${testcase_dir}"

# If the python script returned nonzero, exit with the same status code
if [ $? -ne 0 ]; then
    exit $?
fi

if [ -z "$(ls -A $minimized_testcase_dir)" ]; then
cat << EOF

Minimized testcase directory is empty!
Are you building with use_libfuzzer=true ?

EOF
    exit 1
fi

cat << EOF

Please test the corpus in $minimized_testcase_dir with $fuzzer_name and confirm it works as expected.

Then, run the following command to upload new testcases to the seed corpus:

    gsutil -m rsync $minimized_testcase_dir gs://clusterfuzz-corpus/libfuzzer/${fuzzer_name}/

    WARNING: Add [-d] argument delete all GCS files that are not also in $minimized_testcase_dir

EOF
