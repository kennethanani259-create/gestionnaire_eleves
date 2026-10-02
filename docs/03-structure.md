# Étape 3 — Arborescence du projet

```
gestionnaire_eleves/
│
├── backend/
│   ├── CMakeLists.txt              # build : lib statique + serveur + tests
│   ├── include/                    # en-têtes publics (miroir de src/)
│   │   ├── core/                   # Config, Error, Logger
│   │   ├── models/                 # entités : Student, ClassRoom, Subject, Teacher,
│   │   │                           #            Grade, Attendance, User, SchoolYear
│   │   ├── database/               # Database, Statement, Transaction, Migrator
│   │   ├── repositories/           # interfaces + implémentations SQLite
│   │   ├── services/               # logique métier
│   │   ├── controllers/            # routes REST
│   │   ├── middleware/             # AuthMiddleware, ErrorHandler, RequestLogger
│   │   └── utils/                  # Crypto, Jwt, Validator, Csv, Pdf, DateTime, Json
│   ├── src/                        # implémentations (.cpp), même découpage
│   │   └── main.cpp                # composition root uniquement
│   ├── migrations/                 # 001_init.sql, 002_seed_admin.sql
│   ├── tests/                      # tests unitaires + intégration (doctest)
│   ├── third_party/                # dépendances vendorisées (offline)
│   │   ├── sqlite3/  httplib/  nlohmann/  doctest/
│   └── README.md
│
├── frontend/                       # SPA statique servie par le backend
│   ├── index.html                  # login + shell applicatif
│   ├── css/styles.css
│   ├── js/
│   │   ├── api.js                  # client REST (fetch + JWT)
│   │   ├── router.js               # routage par hash
│   │   ├── charts.js               # graphiques (canvas, sans dépendance)
│   │   ├── components/             # tableaux, modales, formulaires
│   │   └── pages/                  # login, dashboard, students, classes, subjects,
│   │                               # grades, attendance, results, reports, users, settings
│   └── assets/
│
├── docs/
│   ├── 01-architecture.md
│   ├── 02-data-model.md
│   ├── 03-structure.md
│   ├── 04-api.md                   # référence des endpoints + exemples JSON
│   └── 05-deployment.md
│
├── docker/
│   ├── Dockerfile
│   └── docker-compose.yml
│
├── scripts/                        # build.sh, run.sh, test.sh
├── .gitignore
└── README.md
```

## Cibles CMake

| Cible | Type | Contenu |
|---|---|---|
| `sqlite3` | lib statique C | amalgamation vendorisée |
| `gestionnaire_core` | lib statique C++17 | models, database, repositories, services, utils, controllers, middleware |
| `gestionnaire_server` | exécutable | `main.cpp` + `gestionnaire_core` |
| `gestionnaire_tests` | exécutable | `tests/` + `gestionnaire_core` (doctest, enregistré via CTest) |

Découper le code en bibliothèque `gestionnaire_core` permet aux **tests de lier la même logique
métier que le serveur**, sans recompilation ni duplication.
