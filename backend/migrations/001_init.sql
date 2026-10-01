-- =====================================================================
-- Migration 001 — Schéma initial
-- SGBD cible : SQLite 3 (compatible PostgreSQL moyennant les types)
-- =====================================================================

PRAGMA foreign_keys = ON;

-- ---------------------------------------------------------------------
-- Années scolaires
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS school_years (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    label       TEXT    NOT NULL UNIQUE,              -- ex: '2025-2026'
    start_date  TEXT    NOT NULL,                     -- ISO-8601 YYYY-MM-DD
    end_date    TEXT    NOT NULL,
    is_current  INTEGER NOT NULL DEFAULT 0 CHECK (is_current IN (0,1)),
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    CHECK (date(start_date) < date(end_date))
);
CREATE INDEX IF NOT EXISTS idx_school_years_current ON school_years(is_current);

-- ---------------------------------------------------------------------
-- Utilisateurs (authentification)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    username       TEXT    NOT NULL UNIQUE,
    email          TEXT    NOT NULL UNIQUE,
    password_hash  TEXT    NOT NULL,                  -- pbkdf2_sha256$iter$salt$hash
    full_name      TEXT    NOT NULL,
    role           TEXT    NOT NULL CHECK (role IN ('ADMIN','TEACHER','VIEWER')),
    is_active      INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    last_login_at  TEXT,
    created_at     TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at     TEXT    NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_users_role ON users(role);

-- ---------------------------------------------------------------------
-- Enseignants
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS teachers (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER UNIQUE REFERENCES users(id) ON DELETE SET NULL,
    first_name  TEXT NOT NULL CHECK (length(trim(first_name)) > 0),
    last_name   TEXT NOT NULL CHECK (length(trim(last_name))  > 0),
    email       TEXT UNIQUE,
    phone       TEXT,
    speciality  TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at  TEXT NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_teachers_name ON teachers(last_name, first_name);

-- ---------------------------------------------------------------------
-- Classes
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS classes (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT    NOT NULL CHECK (length(trim(name)) > 0),  -- ex: '6e A'
    level           TEXT    NOT NULL,                                 -- ex: '6e'
    school_year_id  INTEGER NOT NULL REFERENCES school_years(id) ON DELETE RESTRICT,
    main_teacher_id INTEGER REFERENCES teachers(id) ON DELETE SET NULL,
    room            TEXT,
    capacity        INTEGER CHECK (capacity IS NULL OR capacity > 0),
    created_at      TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT    NOT NULL DEFAULT (datetime('now')),
    UNIQUE (name, school_year_id)
);
CREATE INDEX IF NOT EXISTS idx_classes_year  ON classes(school_year_id);
CREATE INDEX IF NOT EXISTS idx_classes_level ON classes(level);

-- ---------------------------------------------------------------------
-- Élèves
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS students (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    matricule        TEXT    NOT NULL UNIQUE,            -- identifiant métier, ex: 'STU-2025-0001'
    first_name       TEXT    NOT NULL CHECK (length(trim(first_name)) > 0),
    last_name        TEXT    NOT NULL CHECK (length(trim(last_name))  > 0),
    birth_date       TEXT    NOT NULL,                   -- YYYY-MM-DD
    gender           TEXT    NOT NULL CHECK (gender IN ('M','F')),
    address          TEXT,
    phone            TEXT,
    email            TEXT,
    guardian_name    TEXT,
    guardian_phone   TEXT,
    enrollment_date  TEXT    NOT NULL,
    status           TEXT    NOT NULL DEFAULT 'ACTIVE'
                     CHECK (status IN ('ACTIVE','INACTIVE','TRANSFERRED','EXPELLED')),
    photo_path       TEXT,
    class_id         INTEGER REFERENCES classes(id) ON DELETE SET NULL,
    created_at       TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at       TEXT    NOT NULL DEFAULT (datetime('now'))
);
CREATE INDEX IF NOT EXISTS idx_students_class     ON students(class_id);
CREATE INDEX IF NOT EXISTS idx_students_status    ON students(status);
CREATE INDEX IF NOT EXISTS idx_students_gender    ON students(gender);
CREATE INDEX IF NOT EXISTS idx_students_last_name ON students(last_name, first_name);

-- ---------------------------------------------------------------------
-- Matières (rattachées à une classe)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS subjects (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    name        TEXT    NOT NULL CHECK (length(trim(name)) > 0),
    code        TEXT    NOT NULL,                         -- ex: 'MATH'
    coefficient REAL    NOT NULL DEFAULT 1 CHECK (coefficient > 0 AND coefficient <= 20),
    class_id    INTEGER NOT NULL REFERENCES classes(id)  ON DELETE CASCADE,
    teacher_id  INTEGER REFERENCES teachers(id) ON DELETE SET NULL,
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    UNIQUE (class_id, code)
);
CREATE INDEX IF NOT EXISTS idx_subjects_class   ON subjects(class_id);
CREATE INDEX IF NOT EXISTS idx_subjects_teacher ON subjects(teacher_id);

-- ---------------------------------------------------------------------
-- Notes
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS grades (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id  INTEGER NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    subject_id  INTEGER NOT NULL REFERENCES subjects(id) ON DELETE CASCADE,
    eval_type   TEXT    NOT NULL CHECK (eval_type IN
                 ('HOMEWORK','QUIZ','EXAM','LAB','PROJECT','CONTINUOUS')),
    score       REAL    NOT NULL CHECK (score >= 0),
    max_score   REAL    NOT NULL CHECK (max_score > 0),
    eval_date   TEXT    NOT NULL,
    term        INTEGER NOT NULL DEFAULT 1 CHECK (term BETWEEN 1 AND 3),
    comment     TEXT,
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    CHECK (score <= max_score)
);
CREATE INDEX IF NOT EXISTS idx_grades_student        ON grades(student_id);
CREATE INDEX IF NOT EXISTS idx_grades_subject        ON grades(subject_id);
CREATE INDEX IF NOT EXISTS idx_grades_student_term   ON grades(student_id, term);
CREATE INDEX IF NOT EXISTS idx_grades_date           ON grades(eval_date);

-- ---------------------------------------------------------------------
-- Présences / absences / retards
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS attendance (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    student_id     INTEGER NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    subject_id     INTEGER REFERENCES subjects(id) ON DELETE SET NULL,
    att_date       TEXT    NOT NULL,                      -- YYYY-MM-DD
    att_time       TEXT,                                  -- HH:MM
    status         TEXT    NOT NULL CHECK (status IN
                    ('PRESENT','ABSENT','EXCUSED','LATE')),
    justification  TEXT,
    comment        TEXT,
    created_at     TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at     TEXT    NOT NULL DEFAULT (datetime('now'))
);
-- Unicite d'un releve : un index sur expressions est indispensable car une
-- contrainte UNIQUE classique ne bloquerait pas les doublons des que
-- att_time ou subject_id vaut NULL (en SQL, NULL est toujours distinct de NULL).
CREATE UNIQUE INDEX IF NOT EXISTS idx_attendance_unique
    ON attendance(student_id, att_date, IFNULL(att_time, ''), IFNULL(subject_id, -1));
CREATE INDEX IF NOT EXISTS idx_attendance_student ON attendance(student_id);
CREATE INDEX IF NOT EXISTS idx_attendance_date    ON attendance(att_date);
CREATE INDEX IF NOT EXISTS idx_attendance_status  ON attendance(status);

-- ---------------------------------------------------------------------
-- Déclencheurs de mise à jour automatique de updated_at
-- ---------------------------------------------------------------------
CREATE TRIGGER IF NOT EXISTS trg_students_updated
AFTER UPDATE ON students FOR EACH ROW
BEGIN UPDATE students SET updated_at = datetime('now') WHERE id = OLD.id; END;

CREATE TRIGGER IF NOT EXISTS trg_grades_updated
AFTER UPDATE ON grades FOR EACH ROW
BEGIN UPDATE grades SET updated_at = datetime('now') WHERE id = OLD.id; END;

CREATE TRIGGER IF NOT EXISTS trg_attendance_updated
AFTER UPDATE ON attendance FOR EACH ROW
BEGIN UPDATE attendance SET updated_at = datetime('now') WHERE id = OLD.id; END;

-- ---------------------------------------------------------------------
-- Vues utilitaires (lecture seule)
-- ---------------------------------------------------------------------
-- Moyenne brute (non pondérée) par élève et par matière, sur 20
CREATE VIEW IF NOT EXISTS v_subject_averages AS
SELECT g.student_id,
       g.subject_id,
       s.class_id,
       s.coefficient,
       g.term,
       COUNT(*)                            AS grade_count,
       AVG(g.score * 20.0 / g.max_score)   AS average_20,
       MAX(g.score * 20.0 / g.max_score)   AS best_20,
       MIN(g.score * 20.0 / g.max_score)   AS worst_20
FROM grades g
JOIN subjects s ON s.id = g.subject_id
GROUP BY g.student_id, g.subject_id, g.term;
