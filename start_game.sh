#!/usr/bin/env bash
cd "$(dirname "$0")"

BIN="./rogue_legacy"
if [ ! -f "$BIN" ]; then
    BIN="./build/rogue_legacy_cpp"
fi

if [ ! -f "$BIN" ]; then
    echo "Executable not found. Building with cmake..."
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    BIN="./build/rogue_legacy_cpp"
fi

chmod +x "$BIN"
exec "$BIN" "$@"

