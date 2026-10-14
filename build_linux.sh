#!/usr/bin/env bash
set -e

echo "========================================================"
echo "  Rogue Legacy C++ - Linux Build Script"
echo "========================================================"

if ! command -v cmake &> /dev/null; then
    echo "[ERROR] CMake is not found in PATH!"
    echo "Please install CMake (e.g. sudo apt install cmake build-essential)"
    exit 1
fi

echo "[1/2] Configuring CMake for Release..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo ""
echo "[2/2] Building rogue_legacy_cpp..."
cmake --build build --config Release -j$(nproc)

echo ""
echo "========================================================"
echo "  BUILD SUCCESSFUL!"
echo "========================================================"
if [ -f build/rogue_legacy_cpp ]; then
    cp build/rogue_legacy_cpp rogue_legacy
    chmod +x rogue_legacy
    echo "Executable ready at: ./rogue_legacy"
    echo "Run with: ./rogue_legacy"
fi

