#!/usr/bin/env python3

import subprocess
import sys

env_file, bin, rest = sys.argv[1], sys.argv[2], sys.argv[3:]

# Read the environment block from the file. This is stored in the format used
# by CreateProcess. Drop last 2 NULs, one for list terminator, one for
# trailing vs. separator.
env_pairs = open(env_file).read()[:-2].split('\0')
env_dict = dict([item.split('=', 1) for item in env_pairs])

sys.exit(subprocess.call([bin] + rest, env=env_dict, shell=True))
