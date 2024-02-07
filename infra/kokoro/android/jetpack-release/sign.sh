#!/bin/bash
# Signs WebGPU AAR

set - ex

# Check if the APK exists.
AAR_NAME="webgpu-release.aar"
APK_PATH= find "$KOKORO_GFILE_DIR" -name "$AAR_NAME"
if [[ -z "$AAR_NAME" ]]
then
  echo "Webgpu aar not found"
fi

OUT_DIR="$PWD/out"

mkdir -p "$OUT_DIR"/signed_aar
cd "$OUT_DIR"

chmod 777 signed_aar

# TODO: Change the name to correspond to the Chromium version
/escalated_sign/escalated_sign.py -j /escalated_sign_jobs -t signjar \
 "$AAR_NAME" \
 "$OUT_DIR"/signed_aar/webgpu-release.aar