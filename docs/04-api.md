# Étape 7 — Référence de l'API REST

Base : `http://localhost:8080` · Toutes les réponses sont en `application/json; charset=utf-8`.

## 1. Codes HTTP

| Code | Signification | Exemple |
|---|---|---|
| 200 | Succès | lecture, modification |
| 201 | Ressource créée | `POST /api/students` |
| 204 | Succès sans contenu | `DELETE /api/students/1` |
| 400 | Requête invalide | champ manquant, note hors barème, JSON mal formé |
| 401 | Authentification requise | jeton absent/expiré *(étape 8)* |
| 403 | Droits insuffisants | enseignant tentant une action admin *(étape 8)* |
| 404 | Ressource inexistante | id inconnu, endpoint inconnu |
| 409 | Conflit | matricule dupliqué, classe non vide |
| 500 | Erreur serveur | anomalie interne (détail uniquement dans les logs) |

## 2. Format d'erreur unifié

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Donnees invalides",
    "details": {
      "fields": {
        "first_name": "Ce champ est obligatoire",
        "birth_date": "Date invalide (format attendu: AAAA-MM-JJ)",
        "email": "Adresse e-mail invalide"
      }
    }
  }
}
```

Toutes les erreurs d'un formulaire sont renvoyées **en une seule fois**.

## 3. Endpoints

### Santé
| Méthode | Chemin | Accès |
|---|---|---|
| GET | `/api/health` | public |

### Authentification et inscription

| Méthode | Chemin                   | Accès  | Description                                  |
| ------- | ------------------------ | ------ | -------------------------------------------- |
| POST    | `/api/auth/login`        | public | Connexion, retourne un jeton JWT              |
| GET     | `/api/auth/registration` | public | Politique d'inscription en vigueur            |
| POST    | `/api/auth/register`     | public | Inscription : créer une école ou en rejoindre une |
| GET     | `/api/auth/schools/{code}` | public | Établissement correspondant à un matricule (nom et ville seuls) |
| GET     | `/api/auth/me`           | *connecté* | Profil de l'utilisateur connecté          |
| POST    | `/api/auth/password`     | *connecté* | Changement de son propre mot de passe     |
| GET     | `/api/school`            | *connecté* | Son établissement (matricule visible des seuls administrateurs) |
| POST    | `/api/school/code`       | admin  | Renouvelle le matricule de l'établissement    |

> *connecté* = toute session valide, **y compris un parent**, qui n'atteint
> aucune des routes marquées « viewer ».

#### Connexion avec vérification du rôle

```http
POST /api/auth/login
{ "username": "prof", "password": "Demo1234!", "role": "TEACHER" }
```

Le champ `role` est facultatif. S'il est fourni et que le compte ne le porte
pas, la réponse est `403` et **aucun jeton n'est délivré** : ce paramètre sert à
vérifier, jamais à accorder un privilège.

#### Inscription — parcours 1 : créer son établissement

```http
POST /api/auth/register
Content-Type: application/json

{
  "school_name": "College Les Palmiers",
  "school_city": "Cotonou",
  "username": "direction",
  "email": "direction@palmiers.bj",
  "password": "Rentree2026",
  "full_name": "Adjoa Koffi"
}
```

```json
HTTP/1.1 201 Created
{
  "user": {
    "id": 9, "username": "direction", "email": "direction@palmiers.bj",
    "full_name": "Adjoa Koffi", "role": "ADMIN", "role_label": "Administrateur",
    "is_active": true, "school_id": 3, "created_at": "2026-10-02 08:14:03"
  },
  "school": { "id": 3, "name": "College Les Palmiers", "city": "Cotonou" },
  "pending_approval": false,
  "message": "Compte cree. Vous pouvez vous connecter."
}
```

Le fondateur est administrateur de **sa seule** école, et son registre est vide.
Le matricule n'apparaît pas dans cette réponse publique : il se lit ensuite via
`GET /api/school`.

#### Inscription — parcours 2 : rejoindre un établissement

```http
POST /api/auth/register
{
  "school_code": "KEGSRD6X",
  "role": "PARENT",
  "username": "mme.koffi",
  "email": "koffi@parents.local",
  "password": "Rentree2026",
  "full_name": "Adjoa Koffi"
}
```

Points de vigilance :

- Ni `school_name` ni `school_code` : `400` avec `details.fields.school_code`.
- Matricule inconnu : `400`. La casse est indifférente.
- `role` accepté : `TEACHER`, `PARENT`, `VIEWER` (défaut `VIEWER`).
  **`ADMIN` est refusé par `403`** — seul un administrateur en place promeut.
- `is_active` envoyé par le client est **ignoré** : l'état dépend de
  `APP_SELF_REGISTRATION` (`open` par défaut, donc actif immédiatement).
- Mot de passe : 10 caractères minimum, lettres **et** chiffres, sinon `400` avec
  `details.fields.password`.
- Identifiant ou e-mail déjà utilisé : `400` avec le champ fautif.
- Plus de 5 demandes en 15 minutes depuis la même adresse : `429 TOO_MANY_REQUESTS`.
- Connexion sur un compte non validé (mode `approval`) : `403`.
- Si `APP_SELF_REGISTRATION=off` : `403`, et `GET /api/auth/registration` renvoie
  `{"enabled": false}` pour que la page d'entrée masque l'onglet.

En mode `approval`, la validation se fait par `PUT /api/users/{id}` avec
`"is_active": true` (réservé aux administrateurs), ou le refus par
`DELETE /api/users/{id}`.

### Espace parent

| Méthode | Chemin | Accès | Description |
| --- | --- | --- | --- |
| GET | `/api/parent/children` | *connecté* | Les élèves rattachés à mon compte |
| GET | `/api/parent/children/{id}/results` | *connecté* | Bulletin de mon enfant |
| GET | `/api/parent/children/{id}/grades` | *connecté* | Ses notes (`?term=`) |
| GET | `/api/parent/children/{id}/attendance` | *connecté* | Ses présences (`?from=&to=`) |
| GET | `/api/users/{id}/children` | admin | Enfants rattachés à un compte parent |
| POST | `/api/users/{id}/children` | admin | Rattacher un élève (`{"student_id": 12, "relation": "Mere"}`) |
| DELETE | `/api/users/{id}/children/{studentId}` | admin | Retirer un rattachement |

Chaque route `/api/parent/children/{id}/…` vérifie le lien de filiation avant
toute lecture. Un identifiant d'élève non rattaché renvoie **`404`** et non
`403` : un parent n'a pas à apprendre qu'un élève existe ailleurs dans
l'établissement. Les enveloppes sont identiques à celles des routes
`/api/students/{id}/…`, afin que le même code d'affichage les consomme.

### Cloisonnement des établissements

Toute route authentifiée est filtrée par l'établissement du porteur du jeton :
listes, recherches, agrégats du tableau de bord et accès par identifiant. Un
élève, une classe ou un compte d'une autre école est traité comme inexistant
(`404`). Aucun paramètre de requête ne permet de changer d'établissement : la
portée vient du jeton, jamais du client.

### Élèves
| Méthode | Chemin | Accès | Description |
|---|---|---|---|
| GET | `/api/students` | Lecture | Liste paginée, filtrable, triable |
| GET | `/api/students/{id}` | Lecture | Fiche détaillée |
| POST | `/api/students` | Enseignant | Création (201) |
| PUT | `/api/students/{id}` | Enseignant | Modification |
| DELETE | `/api/students/{id}` | Admin | Suppression (204) |
| GET | `/api/students/{id}/grades` | Lecture | Notes de l'élève |
| POST | `/api/students/{id}/grades` | Enseignant | Ajout d'une note |
| GET | `/api/students/{id}/attendance` | Lecture | Historique + synthèse de présence |
| POST | `/api/students/{id}/attendance` | Enseignant | Relevé de présence |
| GET | `/api/students/{id}/results` | Lecture | Bulletin : moyennes, rang, appréciation |

**Paramètres de `GET /api/students`** : `q` (nom, prénom ou matricule), `class_id`, `level`,
`status`, `gender`, `sort_by` (`last_name|first_name|matricule|birth_date|created_at|status|class`),
`sort_dir` (`asc|desc`), `limit` (≤ 500, défaut 50), `offset`.

### Classes, matières, enseignants, années
| Méthode | Chemin | Accès |
|---|---|---|
| GET/POST | `/api/classes` | Lecture / Admin |
| GET/PUT/DELETE | `/api/classes/{id}` | Lecture / Admin / Admin |
| GET | `/api/classes/{id}/students` | Lecture |
| GET | `/api/classes/{id}/subjects` | Lecture |
| GET | `/api/classes/{id}/ranking` | Lecture |
| GET/POST | `/api/subjects` | Lecture / Admin |
| GET/PUT/DELETE | `/api/subjects/{id}` | Lecture / Admin / Admin |
| GET/POST | `/api/teachers` | Lecture / Admin |
| PUT/DELETE | `/api/teachers/{id}` | Admin |
| GET/POST | `/api/school-years` | Lecture / Admin |

### Notes et présences
| Méthode | Chemin | Accès |
|---|---|---|
| GET/POST | `/api/grades` | Lecture / Enseignant |
| GET/PUT/DELETE | `/api/grades/{id}` | Lecture / Enseignant |
| GET/POST | `/api/attendance` | Lecture / Enseignant |
| PUT/DELETE | `/api/attendance/{id}` | Enseignant |
| POST | `/api/attendance/bulk` | Enseignant — appel d'une classe entière |

## 4. Exemples

### Créer un élève

```bash
curl -X POST http://localhost:8080/api/students \
  -H 'Content-Type: application/json' \
  -d '{"first_name":"Awa","last_name":"Dossou","birth_date":"2012-04-18",
       "gender":"F","class_id":1,"email":"awa@example.org"}'
```

```json
{
  "id": 1,
  "matricule": "STU-2026-0001",
  "first_name": "Awa",
  "last_name": "Dossou",
  "full_name": "Awa Dossou",
  "birth_date": "2012-04-18",
  "gender": "F",
  "status": "ACTIVE",
  "status_label": "Actif",
  "class_id": 1,
  "class_name": "6e A",
  "class_level": "6e",
  "enrollment_date": "2026-10-01"
}
```

Le `matricule` et la `enrollment_date` sont générés s'ils ne sont pas fournis.

### Bulletin d'un élève

`GET /api/students/1/results?term=1`

```json
{
  "student": { "id": 1, "full_name": "Awa Dossou", "matricule": "STU-2026-0001" },
  "subjects": [
    { "subject_name": "Mathematiques", "coefficient": 4.0, "average": 15.0,
      "best": 15.0, "worst": 15.0, "grade_count": 1, "appreciation": "Tres bien" },
    { "subject_name": "Francais", "coefficient": 1.0, "average": 10.0,
      "best": 10.0, "worst": 10.0, "grade_count": 1, "appreciation": "Assez bien" }
  ],
  "general_average": 14.0,
  "total_coefficients": 5.0,
  "rank": 1,
  "class_size": 2,
  "appreciation": "Tres bien",
  "attendance": { "total": 1, "present": 0, "absent": 1, "excused": 0,
                  "late": 0, "attendance_rate": 0.0 }
}
```

> `general_average = (15×4 + 10×1) / 5 = 14,0` — moyenne **pondérée par les coefficients**,
> toutes les notes étant d'abord ramenées sur 20.

### Classement d'une classe

`GET /api/classes/1/ranking?term=1`

```json
{
  "items": [
    { "student_id": 1, "full_name": "Awa Dossou", "average": 18.0, "rank": 1 },
    { "student_id": 3, "full_name": "Bintou Adjovi", "average": 15.0, "rank": 2 },
    { "student_id": 2, "full_name": "Kodjo Houngbo", "average": 15.0, "rank": 2 },
    { "student_id": 4, "full_name": "Moussa Zinsou", "average": 8.0, "rank": 4 }
  ],
  "subject_statistics": [
    { "subject_name": "Mathematiques", "average": 14.0, "best": 18.0, "worst": 8.0,
      "student_count": 4, "grade_count": 4 }
  ]
}
```

Rangs **de compétition** : deux ex æquo partagent le rang 2, le suivant est 4.

### Appel d'une classe

```bash
curl -X POST http://localhost:8080/api/attendance/bulk \
  -H 'Content-Type: application/json' \
  -d '{"class_id":1,"date":"2025-10-06","time":"08:00",
       "entries":[{"student_id":1,"status":"PRESENT"},
                  {"student_id":2,"status":"LATE"}]}'
```

```json
{ "created": 2 }
```

## 5. Valeurs d'énumération

| Champ | Valeurs |
|---|---|
| `gender` | `M`, `F` |
| `status` (élève) | `ACTIVE`, `INACTIVE`, `TRANSFERRED`, `EXPELLED` |
| `eval_type` | `HOMEWORK`, `QUIZ`, `EXAM`, `LAB`, `PROJECT`, `CONTINUOUS` |
| `status` (présence) | `PRESENT`, `ABSENT`, `EXCUSED`, `LATE` |
| `role` | `ADMIN`, `TEACHER`, `VIEWER` |

Les équivalents français sont également acceptés en entrée (`DEVOIR`, `RETARD`, `ACTIF`…) ;
la sortie est toujours normalisée en anglais, accompagnée d'un libellé français
(`status_label`, `eval_type_label`).

## 6. En-têtes de sécurité

Toute réponse porte : `X-Content-Type-Options: nosniff`, `X-Frame-Options: SAMEORIGIN`,
`Referrer-Policy: same-origin`, et les en-têtes CORS nécessaires au frontend.
