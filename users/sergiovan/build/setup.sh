#!/bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cd $SCRIPTPATH

mkdir -p debug
mkdir -p coverage

(
  echo "Setting up debug configuration..."
  cd debug
  CC=clang CXX=clang++ cmake ../.. -GNinja --fresh -DCMAKE_BUILD_TYPE=Debug -DQMK_LOCATION=$HOME/keyboard/qmk/qmk_firmware
)

(
  echo "Setting up coverage configuration..."
  cd coverage
  CC=clang CXX=clang++ cmake ../.. -GNinja --fresh -DCMAKE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug -DQMK_LOCATION=$HOME/keyboard/qmk/qmk_firmware
)