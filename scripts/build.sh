#!/usr/bin/env bash
# Compile le backend C++ (Release par defaut : BUILD_TYPE=Debug ./scripts/build.sh)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
cmake -S "$ROOT/backend" -B "$ROOT/backend/build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$ROOT/backend/build" -j "$(nproc 2>/dev/null || echo 4)"
echo "OK -> $ROOT/backend/build/gestionnaire_server"
