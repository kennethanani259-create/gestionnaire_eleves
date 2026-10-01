#!/usr/bin/env bash
# Lance le serveur API + frontend
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -f "$ROOT/.env" ] && set -a && . "$ROOT/.env" && set +a
[ -x "$ROOT/backend/build/gestionnaire_server" ] || "$ROOT/scripts/build.sh"
exec "$ROOT/backend/build/gestionnaire_server"
