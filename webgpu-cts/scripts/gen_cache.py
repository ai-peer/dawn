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

import argparse
import tarfile
import datetime
import os
import subprocess
import sys
import tempfile

script_root = os.path.dirname(os.path.abspath(sys.argv[0]))
dawn_root = os.path.abspath(os.path.join(script_root, "../.."))
bucket = 'dawn-cts-cache'


def cts_hash():
    deps_path = os.path.join(script_root, '../../third_party/webgpu-cts')
    hash = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=deps_path)
    return hash.decode('UTF-8').strip('\n')


def download_from_bucket(name, dst):
    subprocess.check_output(
        ['gcloud', 'storage', 'cp', 'gs://{}/{}'.format(bucket, name), dst],
        cwd=script_root)


def gen_cache(out_dir):
    # Obtain the current hash of the CTS repo
    hash = cts_hash()

    # Download the cache.tar.gz compressed data from the GCP bucket
    tmpDir = tempfile.TemporaryDirectory()
    cacheTarPath = os.path.join(tmpDir.name, 'cache.tar.gz')
    download_from_bucket(hash + "/data", cacheTarPath)

    # Extract the cache.tar.gz into out_dir
    tar = tarfile.open(cacheTarPath)
    tar.extractall(out_dir)

    # Update timestamps
    now = datetime.datetime.now().timestamp()
    for name in tar.getnames():
        path = os.path.join(out_dir, name)
        os.utime(path, (now, now))
    tar.close()


# Extract the cache for CTS runs.
if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    # TODO(bclayton): Unused. Remove
    parser.add_argument('js_script', help='Path to gen_cache.js')

    parser.add_argument('out_dir', help='Output directory for the cache')
    args = parser.parse_args()

    gen_cache(args.out_dir)
