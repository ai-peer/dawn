#!/bin/bash
# Copyright 2024 The Dawn & Tint Authors
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Script to build the Android archive package

set -e # Fail on any error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd )"
ROOT_DIR="$( cd "${SCRIPT_DIR}/../../../.." >/dev/null 2>&1 && pwd )"

if [ -d "/tmpfs" ]; then
    TMP_DIR=/tmpfs
else
    TMP_DIR=/tmp
fi

CLONE_SRC_DIR="$(pwd)"

ls /bin/depot_path

cd $TMP_DIR
# Install depot tools
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH=$TMP_DIR/depot_tools:$PATH

cd $ROOT_DIR

# run gclient to fetch the dependencies
cp scripts/standalone.gclient .gclient
gclient sync

if [[ $? -ne 0 ]]
then
    echo "FAILURE in fetching deps"
    exit 1
fi

cd tools/android

# Use specified JDK version.  Default is JDK11
sudo add-apt-repository ppa:cwchien/gradle
sudo apt-get update
apt-get install -y openjdk-17-jdk
export JAVA_HOME="$(update-java-alternatives -l | grep "1.17" | head -n 1 | tr -s " " | cut -d " " -f 3)"

# gradle 8.0+ is expected for android library
sudo apt-get install gradle-8.3
sudo update-alternatives --set gradle /usr/lib/gradle/8.3/bin/gradle

# Compile .aar
gradle publishToMavenLocal

if [[ $? -ne 0 ]]
then
    echo "FAILURE in publishing to Maven Local"
    exit 1
fi

echo "Successfully built webgpu aar"
