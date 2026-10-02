-- =====================================================================
-- Migration 002 — Multi-établissements, rôle Parent, liens parent/élève
--
-- SQLite ne sait pas modifier une contrainte (UNIQUE, CHECK) en place :
-- les tables concernées sont donc reconstruites (create / copy / drop /
-- rename). Le Migrator désactive l'intégrité référentielle le temps des
-- migrations et exécute PRAGMA foreign_key_check juste après.
--
-- Les données existantes appartiennent à un seul établissement : elles sont
-- rattachées à une école créée ici, afin qu'aucune ligne ne reste orpheline.
-- =====================================================================

-- ---------------------------------------------------------------------
-- Établissements
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS schools (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    -- Matricule communiqué aux membres pour rejoindre l'établissement.
    -- Alphabet sans caractères ambigus (ni O/0 ni I/1), généré côté serveur.
    code        TEXT    NOT NULL UNIQUE CHECK (length(code) BETWEEN 6 AND 16),
    name        TEXT    NOT NULL CHECK (length(trim(name)) > 0),
    city        TEXT,
    country     TEXT,
    phone       TEXT,
    email       TEXT,
    address     TEXT,
    is_active   INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at  TEXT    NOT NULL DEFAULT (datetime('now'))
);

-- Établissement d'accueil des données déjà saisies. Créé uniquement si la
-- base n'est pas vierge, pour ne pas polluer une installation neuve.
INSERT INTO schools (code, name, city)
SELECT 'ETAB01', 'Etablissement principal', NULL
WHERE EXISTS (SELECT 1 FROM users)
   OR EXISTS (SELECT 1 FROM students);

-- ---------------------------------------------------------------------
-- Utilisateurs : ajout de school_id et du rôle PARENT
--
-- school_id est NULL pour un compte non rattaché (cas transitoire pendant
-- la création d'une école). L'identifiant et l'e-mail restent uniques au
-- niveau global : la connexion se fait sans connaître l'établissement.
-- ---------------------------------------------------------------------
CREATE TABLE users_new (
    id             INTEGER PRIMARY KEY AUTOINCREMENT,
    school_id      INTEGER REFERENCES schools(id) ON DELETE CASCADE,
    username       TEXT    NOT NULL UNIQUE,
    email          TEXT    NOT NULL UNIQUE,
    password_hash  TEXT    NOT NULL,
    full_name      TEXT    NOT NULL,
    role           TEXT    NOT NULL CHECK (role IN ('ADMIN','TEACHER','PARENT','VIEWER')),
    is_active      INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    last_login_at  TEXT,
    created_at     TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at     TEXT    NOT NULL DEFAULT (datetime('now'))
);
INSERT INTO users_new (id, school_id, username, email, password_hash, full_name,
                       role, is_active, last_login_at, created_at, updated_at)
SELECT u.id, (SELECT id FROM schools WHERE code = 'ETAB01'),
       u.username, u.email, u.password_hash, u.full_name,
       u.role, u.is_active, u.last_login_at, u.created_at, u.updated_at
FROM users u;
DROP TABLE users;
ALTER TABLE users_new RENAME TO users;
CREATE INDEX IF NOT EXISTS idx_users_role   ON users(role);
CREATE INDEX IF NOT EXISTS idx_users_school ON users(school_id);

-- ---------------------------------------------------------------------
-- Années scolaires : le libellé n'est unique qu'au sein d'un établissement
-- ---------------------------------------------------------------------
CREATE TABLE school_years_new (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    school_id   INTEGER NOT NULL REFERENCES schools(id) ON DELETE CASCADE,
    label       TEXT    NOT NULL,
    start_date  TEXT    NOT NULL,
    end_date    TEXT    NOT NULL,
    is_current  INTEGER NOT NULL DEFAULT 0 CHECK (is_current IN (0,1)),
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    CHECK (date(start_date) < date(end_date)),
    UNIQUE (school_id, label)
);
INSERT INTO school_years_new (id, school_id, label, start_date, end_date,
                              is_current, created_at)
SELECT y.id, (SELECT id FROM schools WHERE code = 'ETAB01'),
       y.label, y.start_date, y.end_date, y.is_current, y.created_at
FROM school_years y;
DROP TABLE school_years;
ALTER TABLE school_years_new RENAME TO school_years;
CREATE INDEX IF NOT EXISTS idx_school_years_current ON school_years(is_current);
CREATE INDEX IF NOT EXISTS idx_school_years_school  ON school_years(school_id);

-- ---------------------------------------------------------------------
-- Enseignants : e-mail unique par établissement
-- ---------------------------------------------------------------------
CREATE TABLE teachers_new (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    school_id   INTEGER NOT NULL REFERENCES schools(id) ON DELETE CASCADE,
    user_id     INTEGER UNIQUE REFERENCES users(id) ON DELETE SET NULL,
    first_name  TEXT NOT NULL CHECK (length(trim(first_name)) > 0),
    last_name   TEXT NOT NULL CHECK (length(trim(last_name))  > 0),
    email       TEXT,
    phone       TEXT,
    speciality  TEXT,
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at  TEXT NOT NULL DEFAULT (datetime('now'))
);
INSERT INTO teachers_new (id, school_id, user_id, first_name, last_name, email,
                          phone, speciality, created_at, updated_at)
SELECT t.id, (SELECT id FROM schools WHERE code = 'ETAB01'),
       t.user_id, t.first_name, t.last_name, t.email, t.phone, t.speciality,
       t.created_at, t.updated_at
FROM teachers t;
DROP TABLE teachers;
ALTER TABLE teachers_new RENAME TO teachers;
CREATE INDEX IF NOT EXISTS idx_teachers_name   ON teachers(last_name, first_name);
CREATE INDEX IF NOT EXISTS idx_teachers_school ON teachers(school_id);
-- IFNULL : sans cela, deux enseignants sans e-mail seraient toujours distincts.
CREATE UNIQUE INDEX IF NOT EXISTS idx_teachers_email_unique
    ON teachers(school_id, IFNULL(email, ''))
    WHERE email IS NOT NULL;

-- ---------------------------------------------------------------------
-- Classes : rattachement direct à l'établissement
-- ---------------------------------------------------------------------
CREATE TABLE classes_new (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    school_id       INTEGER NOT NULL REFERENCES schools(id) ON DELETE CASCADE,
    name            TEXT    NOT NULL CHECK (length(trim(name)) > 0),
    level           TEXT    NOT NULL,
    school_year_id  INTEGER NOT NULL REFERENCES school_years(id) ON DELETE RESTRICT,
    main_teacher_id INTEGER REFERENCES teachers(id) ON DELETE SET NULL,
    room            TEXT,
    capacity        INTEGER CHECK (capacity IS NULL OR capacity > 0),
    created_at      TEXT    NOT NULL DEFAULT (datetime('now')),
    updated_at      TEXT    NOT NULL DEFAULT (datetime('now')),
    UNIQUE (name, school_year_id)
);
INSERT INTO classes_new (id, school_id, name, level, school_year_id,
                         main_teacher_id, room, capacity, created_at, updated_at)
SELECT c.id, (SELECT id FROM schools WHERE code = 'ETAB01'),
       c.name, c.level, c.school_year_id, c.main_teacher_id, c.room, c.capacity,
       c.created_at, c.updated_at
FROM classes c;
DROP TABLE classes;
ALTER TABLE classes_new RENAME TO classes;
CREATE INDEX IF NOT EXISTS idx_classes_year   ON classes(school_year_id);
CREATE INDEX IF NOT EXISTS idx_classes_level  ON classes(level);
CREATE INDEX IF NOT EXISTS idx_classes_school ON classes(school_id);

-- ---------------------------------------------------------------------
-- Élèves : le matricule n'est unique qu'au sein d'un établissement
-- ---------------------------------------------------------------------
CREATE TABLE students_new (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    school_id        INTEGER NOT NULL REFERENCES schools(id) ON DELETE CASCADE,
    matricule        TEXT    NOT NULL,
    first_name       TEXT    NOT NULL CHECK (length(trim(first_name)) > 0),
    last_name        TEXT    NOT NULL CHECK (length(trim(last_name))  > 0),
    birth_date       TEXT    NOT NULL,
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
    updated_at       TEXT    NOT NULL DEFAULT (datetime('now')),
    UNIQUE (school_id, matricule)
);
INSERT INTO students_new (id, school_id, matricule, first_name, last_name, birth_date,
                          gender, address, phone, email, guardian_name, guardian_phone,
                          enrollment_date, status, photo_path, class_id,
                          created_at, updated_at)
SELECT s.id, (SELECT id FROM schools WHERE code = 'ETAB01'),
       s.matricule, s.first_name, s.last_name, s.birth_date, s.gender, s.address,
       s.phone, s.email, s.guardian_name, s.guardian_phone, s.enrollment_date,
       s.status, s.photo_path, s.class_id, s.created_at, s.updated_at
FROM students s;
DROP TABLE students;
ALTER TABLE students_new RENAME TO students;
CREATE INDEX IF NOT EXISTS idx_students_class     ON students(class_id);
CREATE INDEX IF NOT EXISTS idx_students_status    ON students(status);
CREATE INDEX IF NOT EXISTS idx_students_gender    ON students(gender);
CREATE INDEX IF NOT EXISTS idx_students_last_name ON students(last_name, first_name);
CREATE INDEX IF NOT EXISTS idx_students_school    ON students(school_id);

-- ---------------------------------------------------------------------
-- Rattachement parent -> élève
--
-- Un parent ne voit que les élèves qui lui sont liés ici. Le lien est créé
-- par l'administration de l'établissement, jamais par le parent lui-même.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS parent_students (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER NOT NULL REFERENCES users(id)    ON DELETE CASCADE,
    student_id  INTEGER NOT NULL REFERENCES students(id) ON DELETE CASCADE,
    relation    TEXT,                                    -- ex: 'Mere', 'Pere', 'Tuteur'
    created_at  TEXT    NOT NULL DEFAULT (datetime('now')),
    UNIQUE (user_id, student_id)
);
CREATE INDEX IF NOT EXISTS idx_parent_students_user    ON parent_students(user_id);
CREATE INDEX IF NOT EXISTS idx_parent_students_student ON parent_students(student_id);

-- ---------------------------------------------------------------------
-- Déclencheurs : recréés, les tables d'origine ayant été remplacées
-- ---------------------------------------------------------------------
DROP TRIGGER IF EXISTS trg_students_updated;
CREATE TRIGGER trg_students_updated
AFTER UPDATE ON students FOR EACH ROW
BEGIN UPDATE students SET updated_at = datetime('now') WHERE id = OLD.id; END;

CREATE TRIGGER IF NOT EXISTS trg_schools_updated
AFTER UPDATE ON schools FOR EACH ROW
BEGIN UPDATE schools SET updated_at = datetime('now') WHERE id = OLD.id; END;
