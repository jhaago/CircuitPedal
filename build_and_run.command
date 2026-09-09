#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "========================================"
echo " CircuitPedal V0.10 - Mac terminal build + run"
echo "========================================"
echo

if ! command -v cmake >/dev/null 2>&1; then
  echo "CMake is not installed."
  echo "Install it first (for example: brew install cmake), then run this file again."
  echo
  read -r -p "Press Enter to close..."
  exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Apple Command Line Tools are not installed."
  echo "Run: xcode-select --install"
  echo "Then run this file again."
  echo
  read -r -p "Press Enter to close..."
  exit 1
fi

echo "Building..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo
echo "Running circuit-core validation suite..."
ctest --test-dir build --output-on-failure

echo
echo "Available audio devices:"
./build/circuitpedal --list-devices

echo
echo "IMPORTANT: turn your physical headphone/amp/interface output volume down before continuing."
echo "The default run requests a 64-frame buffer and uses input channel 1 on the default output device."
echo "For another device/channel, run ./build/circuitpedal --help or see README.md."
read -r -p "Press Enter to start the live pedal..."

echo
./build/circuitpedal
