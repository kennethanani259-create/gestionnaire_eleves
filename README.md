# Gestionnaire d'élèves

Logiciel de gestion scolaire : élèves, classes, matières, notes, bulletins, présences,
statistiques et rapports. **Backend C++17 (API REST) + SQLite + frontend web.**

> État d'avancement : étapes 1 → 4 terminées (architecture, modèle de données, arborescence,
> configuration CMake). Les couches métier, API, frontend et tests arrivent aux étapes suivantes.

## Pile technique

| Couche | Technologie |
|---|---|
| Backend | C++17, CMake ≥ 3.16 |
| Serveur HTTP | cpp-httplib 0.15.3 (vendorisé) |
| JSON | nlohmann/json 3.11.3 (vendorisé) |
| Base de données | SQLite 3.38.2 (amalgamation vendorisée) |
| Tests | doctest 2.4.11 + CTest |
| Frontend | HTML/CSS/JavaScript (SPA, sans build step) |

Aucune dépendance système à installer : les 4 bibliothèques tierces sont incluses dans
`backend/third_party/`. Il suffit d'un compilateur C++17 et de CMake.

## Compilation

```bash
./scripts/build.sh                  # Release
BUILD_TYPE=Debug ./scripts/build.sh # Debug
```

ou manuellement :

```bash
cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j
```

> CMake absent ? `pip install cmake ninja` suffit (binaire autonome).

## Tests

```bash
./scripts/test.sh          # équivaut à : ctest --test-dir backend/build --output-on-failure
```

## Lancement

```bash
./scripts/run.sh
```

## Configuration (variables d'environnement)

| Variable | Défaut | Description |
|---|---|---|
| `APP_HOST` | `0.0.0.0` | Interface d'écoute |
| `APP_PORT` | `8080` | Port HTTP |
| `APP_DB_PATH` | `data/school.db` | Fichier SQLite |
| `APP_MIGRATIONS_DIR` | `backend/migrations` | Répertoire des migrations SQL |
| `APP_FRONTEND_DIR` | `frontend` | Répertoire servi en statique |
| `APP_JWT_SECRET` | *(obligatoire en production)* | Secret de signature des JWT |
| `APP_JWT_TTL_MINUTES` | `480` | Durée de validité des jetons |
| `APP_LOG_LEVEL` | `INFO` | `DEBUG`/`INFO`/`WARNING`/`ERROR`/`CRITICAL` |

## Documentation

- [Architecture](docs/01-architecture.md)
- [Modèle de données et schéma SQL](docs/02-data-model.md)
- [Arborescence du projet](docs/03-structure.md)
