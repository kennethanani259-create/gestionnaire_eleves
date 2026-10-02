/// Tests de la couche base de donnees : RAII, binding, transactions, migrations.
#include <doctest/doctest.h>

#include "core/Error.hpp"
#include "database/Database.hpp"
#include "database/Migrator.hpp"
#include "TestHelpers.hpp"

using namespace app;

TEST_CASE("Les migrations creent le schema et sont idempotentes") {
    Database db(":memory:");
    Migrator migrator(db, testing::migrationsDir());

    const int applied = migrator.migrate();
    CHECK(applied >= 1);
    CHECK(migrator.migrate() == 0);  // deuxieme passage : rien a faire

    auto stmt = db.prepare(
        "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name IN "
        "('students','classes','subjects','teachers','grades','attendance','users','school_years');");
    REQUIRE(stmt.step());
    CHECK(stmt.getInt64(0) == 8);
}

TEST_CASE("Le binding parametre neutralise les tentatives d'injection SQL") {
    auto db = testing::makeTestDatabase();
    SchoolYearRepository years(*db);
    ClassRepository classes(*db);
    StudentRepository students(*db);

    const auto yearId = years.create(testing::makeSchoolYear());
    const auto classId = classes.create(testing::makeClass(yearId));
    auto student = testing::makeStudent();
    student.classId = classId;
    students.create(student);

    // Charge utile malveillante traitee comme une simple chaine.
    StudentFilter filter;
    filter.query = std::string("'; DROP TABLE students; --");
    CHECK(students.search(filter).empty());

    auto stmt = db->prepare("SELECT COUNT(*) FROM students;");
    REQUIRE(stmt.step());
    CHECK(stmt.getInt64(0) == 1);  // la table existe toujours
}

TEST_CASE("Une transaction non committee est annulee automatiquement") {
    auto db = testing::makeTestDatabase();
    SchoolYearRepository years(*db);
    const auto yearId = years.create(testing::makeSchoolYear());

    {
        Transaction tx(*db);
        ClassRepository classes(*db);
        classes.create(testing::makeClass(yearId, "5e B"));
        // pas de commit : le destructeur doit annuler
    }

    ClassRepository classes(*db);
    CHECK(classes.findAll({}).empty());
}

TEST_CASE("Une transaction committee persiste les donnees") {
    auto db = testing::makeTestDatabase();
    SchoolYearRepository years(*db);
    const auto yearId = years.create(testing::makeSchoolYear());

    {
        Transaction tx(*db);
        ClassRepository classes(*db);
        classes.create(testing::makeClass(yearId, "4e A"));
        tx.commit();
    }

    ClassRepository classes(*db);
    CHECK(classes.findAll({}).size() == 1);
}

TEST_CASE("Les contraintes de la base sont traduites en exceptions typees") {
    auto db = testing::makeTestDatabase();
    SchoolYearRepository years(*db);
    ClassRepository classes(*db);
    StudentRepository students(*db);

    const auto yearId = years.create(testing::makeSchoolYear());
    const auto classId = classes.create(testing::makeClass(yearId));

    SUBCASE("matricule duplique -> ConflictError (409)") {
        auto a = testing::makeStudent("STU-2025-0001");
        a.classId = classId;
        students.create(a);
        auto b = testing::makeStudent("STU-2025-0001", "Kodjo", "Houngbo");
        b.classId = classId;
        CHECK_THROWS_AS(students.create(b), ConflictError);
    }

    SUBCASE("cle etrangere inexistante -> ValidationError (400)") {
        auto s = testing::makeStudent("STU-2025-0002");
        s.classId = 99999;
        CHECK_THROWS_AS(students.create(s), ValidationError);
    }

    SUBCASE("note superieure au bareme -> ValidationError (400)") {
        auto stmt = db->prepare(
            "INSERT INTO grades(student_id, subject_id, eval_type, score, max_score, eval_date) "
            "VALUES (?,?,?,?,?,?);");
        auto s = testing::makeStudent("STU-2025-0003");
        s.classId = classId;
        const auto studentId = students.create(s);
        SubjectRepository subjects(*db);
        const auto subjectId = subjects.create(testing::makeSubject(classId));
        stmt.bindAll(studentId, subjectId, std::string("EXAM"), 25.0, 20.0,
                     std::string("2025-10-01"));
        CHECK_THROWS_AS(stmt.execute(), ValidationError);
    }
}

TEST_CASE("Une requete SQL invalide leve DatabaseError sans faire planter le processus") {
    auto db = testing::makeTestDatabase();
    CHECK_THROWS_AS(db->prepare("SELECT * FROM table_inexistante;"), DatabaseError);
}
