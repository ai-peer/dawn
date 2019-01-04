#!/bin/sh
pwd=$PWD
cd ../out/Debug
#node $pwd/test.js "$@"
node -e "const dawn = require('${pwd}/build/Debug/dawn');" -i
