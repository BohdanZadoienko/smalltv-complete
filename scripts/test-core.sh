#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
JSON=".pio/libdeps/smalltv_complete/ArduinoJson/src"
${CXX:-c++} -std=c++17 -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -I"$JSON" tests/core_test.cpp -o build/core-test
./build/core-test
