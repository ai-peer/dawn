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

import os, subprocess, sys

from generator_lib import Generator, run_generator, FileRender


def get_git():
    return 'git.bat' if sys.platform == 'win32' else 'git'


def get_gitHash(dawnDir):
    result = subprocess.run([get_git(), 'rev-parse', 'HEAD'],
                            stdout=subprocess.PIPE,
                            cwd=dawnDir)
    if result.returncode == 0:
        return result.stdout.decode('utf-8').strip()
    return ''


def get_gitHead(dawnDir):
    result = subprocess.run(
        [get_git(), 'rev-parse', '--symbolic-full-name', 'HEAD'],
        stdout=subprocess.PIPE,
        cwd=dawnDir)
    if result.returncode == 0:
        path = os.path.join(dawnDir, '.git',
                            result.stdout.decode('utf-8').strip())
        if os.path.exists(path):
            return path
        else:
            raise Exception('Unable to resolve git HEAD hash file:', path)
    raise Exception('Failed to execute git rev-parse to resolve git head.')


def compute_params(args):
    return {
        'get_gitHash': lambda: get_gitHash(os.path.abspath(args.dawn_dir)),
    }


class DawnVersionGenerator(Generator):
    def get_description(self):
        return 'Generates version dependent Dawn code. Currently regenerated dependent on git hash.'

    def add_commandline_arguments(self, parser):
        parser.add_argument('--dawn-dir',
                            required=True,
                            type=str,
                            help='The Dawn root directory path to use')

    def get_dependencies(self, args):
        # Only dependency is the resolved HEAD hash file, this is returned via git command(s).
        return [get_gitHead(os.path.abspath(args.dawn_dir))]

    def get_file_renders(self, args):
        params = compute_params(args)

        return [
            FileRender('dawn/common/Version.h',
                       'src/dawn/common/Version_autogen.h', [params]),
        ]


if __name__ == '__main__':
    sys.exit(run_generator(DawnVersionGenerator()))
