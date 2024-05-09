#!/bin/bash
set -euo pipefail

cat \
    snippets/library_webgpu_struct_info_part1.txt \
    webgpu_struct_info_generated32.json \
    snippets/library_webgpu_struct_info_part2.txt \
    webgpu_struct_info_generated64.json \
    snippets/library_webgpu_struct_info_part3.txt \
    > library_webgpu_struct_info_generated.js
