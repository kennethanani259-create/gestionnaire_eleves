#!/usr/bin/env bash
# Compile puis execute l'ensemble des tests (unitaires + integration)
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"$ROOT/scripts/build.sh"
ctest --test-dir "$ROOT/backend/build" --output-on-failure
