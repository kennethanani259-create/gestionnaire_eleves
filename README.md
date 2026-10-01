# Gestionnaire d'élèves

Application complète de gestion des élèves d'un établissement scolaire :
**backend C++17** (API REST JSON + SQLite) et **frontend web** (SPA sans
dépendance, servie par le backend lui-même).

Le projet est réellement compilable et exécutable : aucune dépendance fictive,
aucune bibliothèque à installer manuellement (les bibliothèques tierces sont
vendorisées dans `backend/third_party`).

---

## 1. Démarrage rapide

```bash
# 1. Compiler (CMake + Ninja)
./scripts/build.sh

# 2. Lancer le serveur (API + interface web)
./scripts/run.sh

# 3. Ouvrir http://localhost:8080
```

Au **premier démarrage**, un compte administrateur est créé automatiquement et
son mot de passe aléatoire est affiché dans les logs :

```
WARNING  | auth | Compte administrateur initial cree
WARNING  | auth |   identifiant : admin
WARNING  | auth |   mot de passe: 0b02413c3ae99bf2a0
```

### Jeu de données de démonstration (facultatif)

Pour disposer immédiatement de classes, matières, élèves, notes et présences :

```bash
python3 scripts/seed_demo.py --password <mot-de-passe-affiche-au-demarrage>
```

Le script passe uniquement par l'API REST. Il fixe le mot de passe
administrateur à une valeur connue et crée trois comptes de démonstration :

| Identifiant | Mot de passe | Rôle          | Peut faire                                     |
|-------------|--------------|---------------|------------------------------------------------|
| `admin`     | `Admin123!`  | Administrateur| Tout, y compris supprimer et gérer les comptes |
| `prof`      | `Demo1234!`  | Enseignant    | Saisir élèves, notes, présences                |
| `lecteur`   | `Demo1234!`  | Lecteur       | Consultation seule                             |

Il génère 3 classes, 18 matières, 24 élèves, ~576 notes et 45 appels.

---

## 2. Fonctionnalités

**Élèves** — fiche complète (identité, contacts, parent/tuteur, statut
ACTIF/INACTIF/TRANSFÉRÉ/EXCLU), matricule automatique `STU-<année>-<n°>`,
recherche instantanée, filtres (classe, statut, sexe), tri, pagination,
confirmation obligatoire avant suppression.

**Classes & matières** — niveaux, année scolaire, enseignant principal,
effectif/capacité, coefficient configurable par matière.

**Notes** — 6 types d'évaluation (devoir, interrogation, examen, TP, projet,
contrôle continu), barème libre ramené sur 20, trimestres 1 à 3, commentaire.

**Calculs (100 % côté backend, en C++)** — moyenne par matière, moyenne
générale pondérée `Σ(moyenne × coef) / Σ(coef)`, classement de type compétition
(1, 2, 2, 4), meilleure et pire note, appréciation automatique, élèves en
difficulté (moyenne < 10).

**Présences** — présent / absent / absent justifié / retard, appel groupé d'une
classe en un écran, justification, historique, taux de présence.

**Tableau de bord** — effectifs, répartition garçons/filles, moyenne générale,
meilleur élève, absences et retards, distribution des moyennes, absences par
mois, statistiques par matière, le tout avec des graphiques SVG.

**Rapports & exports** — bulletin PDF d'un élève (notes, rang, appréciation,
absences), rapport PDF d'une classe, exports CSV/JSON, imports CSV/JSON avec
rapport d'erreurs ligne par ligne.

**Sécurité** — rôles ADMIN / TEACHER / VIEWER vérifiés côté serveur, mots de
passe stockés en PBKDF2-SHA256 salé (120 000 itérations), jetons JWT HS256,
requêtes SQL exclusivement paramétrées, aucune donnée sensible exposée par
l'API.

---

## 3. Prérequis et compilation

| Besoin        | Version                          |
|---------------|----------------------------------|
| Compilateur   | g++ 9+ / clang 10+ (C++17)       |
| CMake         | 3.16+                            |
| Générateur    | Ninja ou Make                    |

Les bibliothèques **cpp-httplib**, **nlohmann/json**, **SQLite** (amalgamation)
et **doctest** sont déjà incluses dans le dépôt : rien à installer.

```bash
./scripts/build.sh              # build Release dans backend/build
BUILD_TYPE=Debug ./scripts/build.sh
./scripts/test.sh               # suite de tests (79 cas, 1021 assertions)
./scripts/run.sh                # lance le serveur
```

Compilation manuelle équivalente :

```bash
cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build -j
```

Le code compile sans aucun avertissement avec `-Wall -Wextra -Wpedantic`.

---

## 4. Configuration (variables d'environnement)

| Variable               | Défaut                | Rôle                                      |
|------------------------|-----------------------|-------------------------------------------|
| `APP_HOST`             | `0.0.0.0`             | Interface d'écoute                        |
| `APP_PORT`             | `8080`                | Port HTTP                                 |
| `APP_DB_PATH`          | `data/app.db`         | Fichier SQLite                            |
| `APP_MIGRATIONS_DIR`   | `backend/migrations`  | Répertoire des migrations SQL             |
| `APP_FRONTEND_DIR`     | `frontend`            | Répertoire servi sur `/`                  |
| `APP_UPLOADS_DIR`      | `data/uploads`        | Fichiers téléversés (photos)              |
| `APP_JWT_SECRET`       | *(aléatoire)*         | **À définir en production**               |
| `APP_JWT_TTL_MINUTES`  | `480`                 | Durée de validité des jetons              |
| `APP_THREAD_POOL_SIZE` | `8`                   | Threads HTTP                              |
| `APP_LOG_LEVEL`        | `INFO`                | `DEBUG`/`INFO`/`WARNING`/`ERROR`/`CRITICAL`|
| `APP_LOG_FILE`         | *(vide)*              | Fichier de log en plus de la console      |
| `APP_SELF_REGISTRATION`| `approval`            | Inscription publique : `off`, `approval`, `open` |

### Inscription depuis la page d'accueil

La page d'entrée propose deux volets : **Se connecter** et **Demander un compte**.
`APP_SELF_REGISTRATION` décide du sort des demandes :

| Valeur     | Effet                                                                     |
| ---------- | ------------------------------------------------------------------------- |
| `off`      | Aucune inscription publique ; l'onglet n'est pas affiché. Seul un administrateur crée les comptes. |
| `approval` | *(défaut)* Le compte est créé **inactif**. Il apparaît dans « Comptes » où un administrateur le valide ou le refuse. La connexion est refusée (403) avant validation. |
| `open`     | Le compte est immédiatement utilisable.                                    |

Dans tous les cas, un compte issu de cette page reçoit **obligatoirement** le rôle
`VIEWER` : le rôle et l'état d'activation envoyés par le navigateur sont ignorés.
Le mot de passe doit faire au moins 10 caractères et mêler lettres et chiffres, et
les demandes sont limitées à 5 par quart d'heure et par adresse source.

> `approval` est le défaut volontairement : un registre scolaire contient des
> données personnelles d'élèves mineurs, une inscription en libre-service ne doit
> pas y donner accès sans décision humaine.

Les migrations SQL sont appliquées automatiquement au démarrage.

---

## 5. Docker

```bash
docker compose -f docker/docker-compose.yml up --build
# puis http://localhost:8080
```

La base SQLite est conservée dans le volume `./data`.

---

## 6. API REST (aperçu)

Toutes les routes exigent `Authorization: Bearer <jeton>`, sauf
`POST /api/auth/login` et `GET /api/health`.

```bash
# Connexion
curl -X POST http://localhost:8080/api/auth/login \
     -H 'Content-Type: application/json' \
     -d '{"username":"admin","password":"Admin123!"}'
# -> {"token":"eyJ...","user":{"id":1,"username":"admin","role":"ADMIN",...}}

# Liste filtrée
curl 'http://localhost:8080/api/students?q=kone&class_id=1&limit=20' \
     -H "Authorization: Bearer $TOKEN"

# Bulletin PDF
curl 'http://localhost:8080/api/reports/students/1/pdf' \
     -H "Authorization: Bearer $TOKEN" -o bulletin.pdf
```

| Méthode | Route | Rôle minimum |
|---|---|---|
| POST | `/api/auth/login` | public |
| GET | `/api/auth/me` · POST `/api/auth/password` | VIEWER |
| GET/POST | `/api/students` | VIEWER / TEACHER |
| GET/PUT/DELETE | `/api/students/{id}` | VIEWER / TEACHER / ADMIN |
| GET/POST | `/api/students/{id}/grades` · `/attendance` | VIEWER / TEACHER |
| GET | `/api/students/{id}/results` | VIEWER |
| GET/POST/PUT/DELETE | `/api/classes`, `/api/subjects`, `/api/teachers` | VIEWER / ADMIN |
| GET | `/api/classes/{id}/ranking` · `/students` · `/subjects` | VIEWER |
| GET/POST/PUT/DELETE | `/api/grades`, `/api/attendance` | VIEWER / TEACHER |
| POST | `/api/attendance/bulk` | TEACHER |
| GET | `/api/dashboard`, `/api/reports` | VIEWER |
| GET | `/api/reports/students/{id}/pdf`, `/api/reports/classes/{id}/pdf` | VIEWER |
| GET | `/api/export/students.csv\|.json`, `/api/export/grades.csv` | VIEWER |
| POST | `/api/import/students/csv\|json` | ADMIN |
| GET/POST/PUT/DELETE | `/api/users`, `/api/users/{id}` | ADMIN |

Codes de retour : `200`, `201`, `204`, `400`, `401`, `403`, `404`, `409`, `500`.
Format d'erreur uniforme :

```json
{"error":{"code":"VALIDATION_ERROR","message":"Donnees invalides",
          "details":{"fields":{"last_name":"Le nom est obligatoire"}}}}
```

Détail complet et exemples : [`docs/04-api.md`](docs/04-api.md).

---

## 7. Structure du projet

```
gestionnaire_eleves/
├── backend/
│   ├── include/            en-têtes (api, controllers, services,
│   │                       repositories, models, middleware, utils)
│   ├── src/
│   │   ├── main.cpp        composition root uniquement
│   │   ├── api/            serveur HTTP, routeur
│   │   ├── controllers/    traduction HTTP <-> services (aucune logique métier)
│   │   ├── services/       règles métier, calculs, rapports
│   │   ├── repositories/   accès SQL paramétré
│   │   ├── database/       connexion SQLite, migrations, transactions
│   │   ├── middleware/     authentification et permissions
│   │   ├── models/         entités et sérialisation JSON
│   │   └── utils/          crypto, JWT, CSV, PDF, logs, validation
│   ├── migrations/         schéma SQL versionné
│   ├── tests/              79 cas de test (doctest)
│   └── CMakeLists.txt
├── frontend/               SPA : index.html, css/, js/ (api, ui, charts, pages)
├── docs/                   architecture, modèle de données, API
├── docker/                 Dockerfile + docker-compose.yml
└── scripts/                build.sh, test.sh, run.sh, seed_demo.py
```

Architecture en couches stricte :
`contrôleur → service → repository → base`. Un contrôleur ne contient jamais de
SQL ni de règle métier ; un service ne connaît jamais HTTP.

---

## 8. Base de données

SQLite (fichier unique, persistant), 8 tables avec clés étrangères, contraintes
et index : `school_years`, `teachers`, `classes`, `subjects`, `students`,
`grades`, `attendance`, `users`.

Mode `WAL`, `foreign_keys=ON`, `busy_timeout=5000`, accès `FULLMUTEX` pour le
multi-threading. Schéma complet : [`docs/02-data-model.md`](docs/02-data-model.md).

Le passage à PostgreSQL est prévu : les repositories sont derrière des
interfaces (`IStudentRepository`, …), seule la couche `database/` serait à
doubler.

---

## 9. Tests

```bash
./scripts/test.sh
# [doctest] test cases:   79 |   79 passed | 0 failed
# [doctest] assertions: 1021 | 1021 passed | 0 failed
```

Couverture : connexion et migrations de la base, repositories (CRUD, filtres,
pagination), validation, services (moyennes, classement, taux de présence),
cryptographie (vecteurs officiels FIPS 180-4, RFC 4231, RFC 7914), JWT (jetons
falsifiés, expirés, `alg:none`), authentification et autorisations, et API REST
de bout en bout (codes HTTP, PDF, exports, imports).

---

## 10. Évolutivité prévue

Multi-établissements (ajout d'une table `schools` + clé étrangère), multi-années
(déjà en place via `school_years`), espace parents (réutilise `users` et les
rôles), paiements, emplois du temps et notifications (nouvelles tables +
services, sans impact sur l'existant).

---

## Licence

Projet pédagogique fourni tel quel.
