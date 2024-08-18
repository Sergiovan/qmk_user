#!/bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cd $SCRIPTPATH

cmake --build debug/
ctest --test-dir debug/ || ctest --test-dir debug/ --rerun-failed --output-on-failure