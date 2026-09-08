#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "========================================"
echo " CircuitPedal V0.1 - Mac build + run"
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
echo "Running circuit-core self-test..."
./build/circuit_core_test

echo
echo "IMPORTANT: turn your physical headphone/amp/interface output volume down before continuing."
echo "Use the same audio interface for guitar input and audio output in V0.1."
read -r -p "Press Enter to start the live pedal..."

echo
./build/circuitpedal
