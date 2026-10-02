#!/usr/bin/env bash
# Prepare un environnement de demonstration pret a demarrer :
#   1. outillage de compilation (CMake/Ninja) s'il manque
#   2. compilation du backend
#   3. base SQLite peuplee de donnees de demonstration si elle est absente
#
# Le serveur n'est PAS lance par ce script : il se contente de rendre
# `./scripts/run.sh` immediatement utilisable. Idempotent.
set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$(pwd)"

DB_PATH="${APP_DB_PATH:-data/dev.db}"
PORT="${APP_PORT:-8080}"
DEMO_PASSWORD="${DEMO_PASSWORD:-Admin123!}"

export PATH="$PATH:$HOME/.local/bin"

# --- 1. Outillage ----------------------------------------------------------
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
  echo "==> Installation de CMake et Ninja"
  pip3 install --quiet --break-system-packages cmake ninja
fi
echo "==> $(cmake --version | head -1)"

# --- 2. Compilation --------------------------------------------------------
if [ ! -x backend/build/gestionnaire_server ]; then
  echo "==> Compilation du backend (quelques minutes au premier passage)"
  ./scripts/build.sh
else
  echo "==> Binaire deja present, compilation ignoree"
fi

# --- 3. Donnees de demonstration ------------------------------------------
if [ -f "$DB_PATH" ]; then
  echo "==> Base deja presente ($DB_PATH), amorcage ignore"
  echo
  echo "Pret. Lancez : ./scripts/run.sh"
  exit 0
fi

echo "==> Base absente : creation et amorcage"
LOG="$(mktemp)"
APP_HOST=127.0.0.1 APP_PORT="$PORT" APP_DB_PATH="$DB_PATH" \
  APP_MIGRATIONS_DIR=backend/migrations APP_FRONTEND_DIR=frontend \
  APP_LOG_LEVEL=INFO APP_JWT_SECRET="${APP_JWT_SECRET:-dev-secret-de-developpement-123456}" \
  ./backend/build/gestionnaire_server >"$LOG" 2>&1 &
SERVER_PID=$!
trap 'kill "$SERVER_PID" 2>/dev/null || true; rm -f "$LOG"' EXIT

# Attendre que le serveur reponde (plutot qu'un sleep arbitraire).
for _ in $(seq 1 60); do
  if curl -fsS "http://127.0.0.1:$PORT/api/health" >/dev/null 2>&1; then break; fi
  sleep 0.5
done

# Le mot de passe administrateur initial est aleatoire et journalise au
# premier demarrage : on le relit pour pouvoir amorcer via l'API.
INITIAL_PASSWORD="$(grep -oE 'mot de passe: [A-Za-z0-9]+' "$LOG" | tail -1 | awk '{print $4}')"
if [ -z "$INITIAL_PASSWORD" ]; then
  echo "ERREUR : mot de passe administrateur initial introuvable dans le journal." >&2
  cat "$LOG" >&2
  exit 1
fi

python3 scripts/seed_demo.py --url "http://127.0.0.1:$PORT" \
                             --password "$INITIAL_PASSWORD" \
                             --new-password "$DEMO_PASSWORD"

echo
echo "Pret. Lancez : ./scripts/run.sh"
echo "Connexion : admin / $DEMO_PASSWORD"
