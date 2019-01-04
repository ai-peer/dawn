#!/bin/sh
pwd=$PWD
cd ../out/Debug
node $pwd/test.js "$@"
