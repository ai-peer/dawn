#!/bin/bash
set -euo pipefail

source ~/sources/emsdk/emsdk_env.sh

gen_struct_info=../../emsdk/upstream/emscripten/tools/gen_struct_info.py
include_dir=../../../../out/canvaskit_wasm/gen/third_party/externals/dawn/emscripten/include
json=../../../../out/canvaskit_wasm/gen/third_party/externals/dawn/emscripten/webgpu_struct_info.json

"$gen_struct_info" "$json" -I "$include_dir" -o webgpu_struct_info_generated32.json
"$gen_struct_info" "$json" -I "$include_dir" -o webgpu_struct_info_generated64.json --wasm64
