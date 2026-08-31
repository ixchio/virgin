#!/usr/bin/env bash
set -e
# Virgin Browser build helper (Linux-first)
# Requires: CMake, Ninja, Qt 6.8+ WebEngine and SQLite driver

if ! command -v cmake &>/dev/null; then
  echo "cmake not found — install CMake, Ninja, Qt 6.8+ WebEngine, and the Qt SQLite plugin"
  exit 1
fi

# Install deps hint if Qt not found
if ! pkg-config --exists Qt6WebEngineWidgets 2>/dev/null && [ ! -d "/usr/include/qt6" ]; then
  echo "Qt6 WebEngine not detected. Install on Ubuntu:"
  echo "  sudo apt update && sudo apt install -y cmake ninja-build qt6-base-dev qt6-webengine-dev libqt6sql6-sqlite libsqlite3-dev"
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVIRGIN_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
echo ""
echo "Built: ./build/virgin"
echo "Run:   ./build/virgin"
echo "Build and tests complete."
