#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "========================================"
echo " CircuitPedal V0.16 - Circuit Lab build + run"
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

echo "Building CircuitPedal and the macOS test GUI..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

echo
echo "Running circuit-core validation suite..."
ctest --test-dir build --output-on-failure

echo
echo "Validation passed. Launching CircuitPedalGUI."
echo "IMPORTANT: start with your interface/headphone/amp volume LOW."
open -n "build/CircuitPedalGUI.app"
