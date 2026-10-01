# Étape 2 — Modèle de données et schéma SQL

Script complet : [`backend/migrations/001_init.sql`](../backend/migrations/001_init.sql)
(`002_seed_admin.sql` crée le compte administrateur initial, sans données fictives d'élèves).

## 1. Diagramme relationnel

```
school_years 1 ──< classes >── 1 teachers (enseignant principal)
                     │ 1
                     ├──< students        (class_id, SET NULL si classe supprimée)
                     └──< subjects >── 1 teachers (enseignant responsable)
                                │ 1
students 1 ──< grades >─────────┘
students 1 ──< attendance >── 0..1 subjects
users    1 ──0..1 teachers
```

`A 1 ──< B` : un A possède plusieurs B.

## 2. Tables

| Table | Rôle | Clés étrangères | Contraintes notables |
|---|---|---|---|
| `school_years` | Années scolaires | — | `label` unique, `start_date < end_date` |
| `users` | Comptes & authentification | — | `username`/`email` uniques, `role ∈ {ADMIN,TEACHER,VIEWER}` |
| `teachers` | Enseignants | `user_id → users` (SET NULL) | noms non vides, `email` unique |
| `classes` | Classes | `school_year_id` (RESTRICT), `main_teacher_id` (SET NULL) | `(name, school_year_id)` unique |
| `students` | Élèves | `class_id → classes` (SET NULL) | `matricule` unique, `gender ∈ {M,F}`, `status ∈ {ACTIVE,INACTIVE,TRANSFERRED,EXPELLED}` |
| `subjects` | Matières d'une classe | `class_id` (CASCADE), `teacher_id` (SET NULL) | `(class_id, code)` unique, `0 < coefficient ≤ 20` |
| `grades` | Notes | `student_id` (CASCADE), `subject_id` (CASCADE) | `score ≥ 0`, `max_score > 0`, `score ≤ max_score`, `term ∈ [1,3]` |
| `attendance` | Présences | `student_id` (CASCADE), `subject_id` (SET NULL) | `status ∈ {PRESENT,ABSENT,EXCUSED,LATE}`, unicité `(élève, date, heure, matière)` |

## 3. Index

Créés sur toutes les colonnes de jointure et de filtrage :
`students(class_id | status | gender | last_name, first_name)`,
`grades(student_id | subject_id | (student_id, term) | eval_date)`,
`attendance(student_id | att_date | status)`,
`subjects(class_id | teacher_id)`, `classes(school_year_id | level)`.
→ recherche et agrégats restent rapides sur plusieurs milliers d'élèves.

## 4. Règles de calcul (implémentées côté backend)

Toutes les notes sont **ramenées sur 20** avant agrégation : `note_20 = score × 20 / max_score`.

1. **Moyenne par matière** `M_s` = moyenne arithmétique des `note_20` de l'élève dans la matière
   (filtrée par trimestre si demandé).
2. **Moyenne générale pondérée** `M = Σ(M_s × coef_s) / Σ(coef_s)`, sur les matières où l'élève
   possède au moins une note.
3. **Classement** : tri décroissant des `M` des élèves `ACTIVE` de la classe ; en cas d'égalité
   stricte, même rang (rang « compétition » : 1, 2, 2, 4).
4. **Appréciation** : ≥16 Excellent · ≥14 Très bien · ≥12 Bien · ≥10 Assez bien · ≥8 Insuffisant · <8 Très insuffisant.
5. **Taux de présence** = `(PRESENT + LATE) / total_sessions × 100`.
6. **Élève en difficulté** : moyenne générale `< 10`.

Les valeurs ne sont jamais dénormalisées en base : elles sont recalculées à la demande
(cohérence garantie), les agrégats lourds étant délégués à SQL (`v_subject_averages`).

## 5. Entités C++ correspondantes

`Student`, `ClassRoom`, `Subject`, `Teacher`, `Grade`, `Attendance`, `User`, `SchoolYear`
(`backend/include/models/`). Chaque entité possède `to_json()` / `from_json()` et
**n'expose jamais** `password_hash`.
