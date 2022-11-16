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
import logging
import os
import shutil
import sys
import tempfile

from dir_paths import node_dir

from compile_src import compile_src_for_node


def gen_cache(out_dir, js_out_dir=None):
    if js_out_dir is None:
        js_out_dir = tempfile.mkdtemp()
        delete_js_out_dir = True
    else:
        delete_js_out_dir = False

    try:
        logging.info('WebGPU CTS: Transpiling tools...')
        # TODO(crbug.com/dawn/1395): Bring back usage of an incremental build to
        # speed up this operation. It was disabled due to flakiness.
        compile_src_for_node(js_out_dir)

        old_sys_path = sys.path
        try:
            sys.path = old_sys_path + [node_dir]
            from node import RunNode
        finally:
            sys.path = old_sys_path

        # Save the cwd. gen_cache.js needs to be run from a specific directory.
        # TODO: fix this?
        cwd = os.getcwd()
        basedir = os.path.basename(js_out_dir)
        os.chdir(os.path.join(js_out_dir, '..'))
        return RunNode([
            os.path.join(basedir, 'common', 'tools', 'gen_cache.js'),
            os.path.join(cwd, out_dir),
            os.path.join(basedir, 'webgpu')
        ])

    finally:
        if delete_js_out_dir:
            shutil.rmtree(js_out_dir)


# List all testcases matching a test query.
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('out_dir', help='Output directory for the cache')
    parser.add_argument(
        '--js-out-dir',
        default=None,
        help='Output directory for intermediate compiled JS sources')
    args = parser.parse_args()

    print(gen_cache(args.out_dir, args.js_out_dir))
