#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build sim/out
node sim/tint_test.cjs sim/out/tint_test.inc
GFX=".pio/libdeps/smalltv_complete/Adafruit GFX Library"
JSON=".pio/libdeps/smalltv_complete/ArduinoJson/src"
${CXX:-c++} -O1 -std=c++17 -DARDUINO=100 \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 \
  -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 \
  -Isim/shim -I"$GFX" -I"$JSON" \
  sim/sim.cpp smalltv-pomodoro/ui.cpp smalltv-pomodoro/face_*.cpp \
  smalltv-pomodoro/facepack.cpp smalltv-pomodoro/fonts.cpp \
  smalltv-pomodoro/src/qrcodegen/qrcodegen.cpp "$GFX/Adafruit_GFX.cpp" \
  -o build/renderer-test
cd sim
../build/renderer-test
node png.js
