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
