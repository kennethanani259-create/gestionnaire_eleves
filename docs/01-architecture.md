# Étape 1 — Architecture technique

## 1. Vue d'ensemble

Application client/serveur en 3 tiers :

```
┌──────────────────────┐      HTTP/JSON       ┌───────────────────────────┐
│  Frontend SPA        │  ─────────────────▶  │  Backend C++17 (REST API) │
│  (HTML/CSS/JS)       │  ◀─────────────────  │  cpp-httplib + nlohmann   │
└──────────────────────┘    JWT Bearer        └─────────────┬─────────────┘
                                                            │ SQL paramétré
                                                  ┌─────────▼─────────┐
                                                  │ SQLite 3 (fichier)│
                                                  └───────────────────┘
```

Le frontend est servi en statique par le backend lui‑même (répertoire `frontend/`),
ce qui supprime tout problème de CORS et simplifie le déploiement (un seul binaire + un dossier).

## 2. Couches du backend

Flux d'une requête :

```
HTTP Request
   │
   ▼
[ Middleware ]   logging, auth (JWT), CORS, gestion globale des erreurs
   │
   ▼
[ Controller ]   parsing JSON, codes HTTP, sérialisation DTO  — AUCUNE logique métier
   │
   ▼
[ Service ]      règles métier, validation, calculs (moyennes, rangs, stats), transactions
   │
   ▼
[ Repository ]   CRUD SQL paramétré, mapping ligne ⇄ entité — AUCUNE règle métier
   │
   ▼
[ Database ]     wrapper RAII SQLite (Connection, Statement, Transaction)
```

Règles d'architecture appliquées :

- **Dépendances orientées vers l'intérieur** : Controller → Service → Repository → Database.
  Un Repository n'appelle jamais un Service ; un Service n'inclut jamais `httplib.h`.
- **Injection de dépendances par constructeur** (références/`shared_ptr`), pas de singleton global
  hormis le logger.
- **Interfaces de repository** (`IStudentRepository`, …) : permet de substituer une implémentation
  PostgreSQL ou un mock de test sans toucher aux services.
- `main.cpp` ne fait que : charger la config, ouvrir la DB, appliquer les migrations,
  câbler les dépendances, enregistrer les routes, démarrer le serveur. **Zéro logique métier.**

## 3. Modules transverses

| Module | Rôle |
|---|---|
| `utils/Logger` | Logs horodatés, niveaux DEBUG/INFO/WARNING/ERROR/CRITICAL, contexte, thread-safe |
| `utils/Validator` | Validation réutilisable (email, date ISO, bornes numériques, non-vide) |
| `utils/Crypto` | SHA-256, HMAC-SHA256, PBKDF2-HMAC-SHA256, base64url, génération de sel (CSPRNG) |
| `utils/Jwt` | Encode/décode JWT HS256 (`exp`, `sub`, `role`) |
| `utils/Csv` | Parser/writer CSV conforme RFC 4180 (import/export) |
| `utils/Pdf` | Générateur PDF minimal (bulletins, rapports) sans dépendance externe |
| `core/Error` | Hiérarchie d'exceptions (`ValidationError` 400, `AuthError` 401, `ForbiddenError` 403, `NotFoundError` 404, `ConflictError` 409) traduites en réponses JSON par le middleware |

## 4. Sécurité

- Mots de passe : **PBKDF2-HMAC-SHA256**, 120 000 itérations, sel aléatoire 16 o, stockage
  `pbkdf2_sha256$iterations$salt_b64$hash_b64`. Comparaison en temps constant.
- Toutes les requêtes SQL sont **préparées et liées** (`sqlite3_bind_*`) → pas d'injection SQL.
- Authentification par **JWT HS256** signé avec un secret issu de l'environnement (`APP_JWT_SECRET`),
  expiration configurable.
- Autorisation **côté serveur** par middleware de rôle (`ADMIN`, `TEACHER`, `VIEWER`) appliqué
  route par route ; le frontend ne fait que masquer l'UI (jamais une garantie de sécurité).
- Les hashs, sels et secrets ne sont **jamais** sérialisés dans une réponse API.
- Clés étrangères SQLite activées (`PRAGMA foreign_keys=ON`), mode WAL, `busy_timeout`.

## 5. Évolutivité prévue

Le schéma et les couches anticipent : multi-établissements (`schools`), multi-années
(`school_years` déjà présent), parents, paiements, emplois du temps, notifications.
Ces extensions se feront par ajout de migrations + repository + service, sans refonte.

## 6. Performance

- Index sur toutes les colonnes de recherche/jointure (voir étape 2).
- Recherche élèves : filtrage **en SQL** (`LIKE` indexé + pagination `LIMIT/OFFSET`), jamais en mémoire.
- Calculs d'agrégats (moyennes, rangs) exécutés en SQL avec agrégation, puis pondérés en C++.
