/// Tests d'integration des repositories (CRUD, recherche, filtres, agregats).
#include <doctest/doctest.h>

#include "core/Error.hpp"
#include "repositories/AttendanceRepository.hpp"
#include "repositories/GradeRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "TestHelpers.hpp"

using namespace app;

namespace {

/// Jeu de donnees minimal : une annee, une classe, deux matieres.
struct Fixture {
    std::unique_ptr<Database> db = testing::makeTestDatabase();
    SchoolYearRepository years{*db};
    ClassRepository classes{*db};
    SubjectRepository subjects{*db};
    StudentRepository students{*db};
    GradeRepository grades{*db};
    AttendanceRepository attendance{*db};
    UserRepository users{*db};

    long long yearId = 0;
    long long classId = 0;
    long long mathId = 0;
    long long frenchId = 0;

    Fixture() {
        yearId = years.create(testing::makeSchoolYear());
        classId = classes.create(testing::makeClass(yearId));
        mathId = subjects.create(testing::makeSubject(classId, "Mathematiques", "MATH", 4.0));
        frenchId = subjects.create(testing::makeSubject(classId, "Francais", "FR", 3.0));
    }
};

}  // namespace

TEST_CASE("StudentRepository : cycle complet creation / lecture / modification / suppression") {
    Fixture f;
    auto student = testing::makeStudent("STU-2025-0001", "Awa", "Dossou");
    student.classId = f.classId;
    student.email = std::string("awa.dossou@example.org");

    const auto id = f.students.create(student);
    CHECK(id > 0);

    auto loaded = f.students.findById(id);
    REQUIRE(loaded.has_value());
    CHECK(loaded->firstName == "Awa");
    CHECK(loaded->className == "6e A");          // champ derive par jointure
    CHECK(loaded->email == std::string("awa.dossou@example.org"));
    CHECK(toString(loaded->status) == "ACTIVE");

    loaded->lastName = "Dossou-Kone";
    loaded->status = StudentStatus::Transferred;
    f.students.update(*loaded);

    auto updated = f.students.findById(id);
    REQUIRE(updated.has_value());
    CHECK(updated->lastName == "Dossou-Kone");
    CHECK(toString(updated->status) == "TRANSFERRED");

    CHECK(f.students.remove(id));
    CHECK_FALSE(f.students.findById(id).has_value());
    CHECK_FALSE(f.students.remove(id));  // deja supprime
}

TEST_CASE("StudentRepository : modifier un eleve inexistant leve NotFoundError") {
    Fixture f;
    auto ghost = testing::makeStudent("STU-2025-9999");
    ghost.id = 4242;
    CHECK_THROWS_AS(f.students.update(ghost), NotFoundError);
}

TEST_CASE("StudentRepository : recherche, filtres, tri et pagination") {
    Fixture f;
    const auto otherClassId = f.classes.create(testing::makeClass(f.yearId, "5e B"));

    auto a = testing::makeStudent("STU-2025-0001", "Awa", "Dossou");
    a.classId = f.classId;
    a.gender = Gender::Female;
    auto b = testing::makeStudent("STU-2025-0002", "Kodjo", "Houngbo");
    b.classId = f.classId;
    b.gender = Gender::Male;
    auto c = testing::makeStudent("STU-2025-0003", "Bintou", "Adjovi");
    c.classId = otherClassId;
    c.gender = Gender::Female;
    c.status = StudentStatus::Inactive;
    f.students.create(a);
    f.students.create(b);
    f.students.create(c);

    SUBCASE("recherche par nom partiel") {
        StudentFilter filter;
        filter.query = std::string("houng");
        const auto found = f.students.search(filter);
        REQUIRE(found.size() == 1);
        CHECK(found[0].firstName == "Kodjo");
    }

    SUBCASE("recherche par matricule") {
        StudentFilter filter;
        filter.query = std::string("STU-2025-0003");
        CHECK(f.students.search(filter).size() == 1);
    }

    SUBCASE("recherche sur nom complet") {
        StudentFilter filter;
        filter.query = std::string("Awa Dossou");
        CHECK(f.students.search(filter).size() == 1);
    }

    SUBCASE("filtre par classe") {
        StudentFilter filter;
        filter.classId = f.classId;
        CHECK(f.students.search(filter).size() == 2);
        CHECK(f.students.count(filter) == 2);
    }

    SUBCASE("filtre par sexe et statut") {
        StudentFilter filter;
        filter.gender = Gender::Female;
        CHECK(f.students.count(filter) == 2);
        filter.status = StudentStatus::Active;
        CHECK(f.students.count(filter) == 1);
    }

    SUBCASE("filtre par niveau de classe") {
        StudentFilter filter;
        filter.level = std::string("6e");
        CHECK(f.students.count(filter) == 2);
    }

    SUBCASE("tri croissant et decroissant par nom") {
        StudentFilter filter;
        auto asc = f.students.search(filter);
        REQUIRE(asc.size() == 3);
        CHECK(asc.front().lastName == "Adjovi");
        filter.descending = true;
        CHECK(f.students.search(filter).front().lastName == "Houngbo");
    }

    SUBCASE("colonne de tri inconnue : repli sur le defaut, pas d'injection") {
        StudentFilter filter;
        filter.sortBy = "last_name; DROP TABLE students";
        CHECK(f.students.search(filter).size() == 3);
    }

    SUBCASE("pagination") {
        StudentFilter filter;
        filter.limit = 2;
        CHECK(f.students.search(filter).size() == 2);
        filter.offset = 2;
        CHECK(f.students.search(filter).size() == 1);
        CHECK(f.students.count(filter) == 3);  // le total ignore la pagination
    }

    SUBCASE("eleves actifs d'une classe") {
        CHECK(f.students.findByClass(f.classId, true).size() == 2);
        CHECK(f.students.findByClass(otherClassId, true).empty());
        CHECK(f.students.findByClass(otherClassId, false).size() == 1);
    }
}

TEST_CASE("StudentRepository : generation de matricules sequentiels") {
    Fixture f;
    CHECK(f.students.nextMatricule(2026) == "STU-2026-0001");
    auto s = testing::makeStudent("STU-2026-0001");
    s.classId = f.classId;
    f.students.create(s);
    CHECK(f.students.nextMatricule(2026) == "STU-2026-0002");
    CHECK(f.students.matriculeExists("STU-2026-0001"));
    CHECK_FALSE(f.students.matriculeExists("STU-2026-0002"));
}

TEST_CASE("ClassRepository : CRUD et compteurs derives") {
    Fixture f;
    auto s = testing::makeStudent("STU-2025-0010");
    s.classId = f.classId;
    f.students.create(s);

    auto loaded = f.classes.findById(f.classId);
    REQUIRE(loaded.has_value());
    CHECK(loaded->studentCount == 1);
    CHECK(loaded->subjectCount == 2);
    CHECK(loaded->schoolYearLabel == std::string("2025-2026"));

    loaded->name = "6e A bis";
    f.classes.update(*loaded);
    CHECK(f.classes.findById(f.classId)->name == "6e A bis");

    ClassFilter filter;
    filter.level = std::string("6e");
    CHECK(f.classes.findAll(filter).size() == 1);
    filter.level = std::string("Terminale");
    CHECK(f.classes.findAll(filter).empty());

    CHECK(f.classes.exists(f.classId));
    CHECK_FALSE(f.classes.exists(9999));
}

TEST_CASE("ClassRepository : nom de classe duplique dans la meme annee -> conflit") {
    Fixture f;
    CHECK_THROWS_AS(f.classes.create(testing::makeClass(f.yearId, "6e A")), ConflictError);
}

TEST_CASE("SubjectRepository : CRUD et coefficients configurables") {
    Fixture f;
    auto math = f.subjects.findById(f.mathId);
    REQUIRE(math.has_value());
    CHECK(math->coefficient == doctest::Approx(4.0));
    CHECK(math->className == std::string("6e A"));

    math->coefficient = 5.0;
    f.subjects.update(*math);
    CHECK(f.subjects.findById(f.mathId)->coefficient == doctest::Approx(5.0));

    CHECK(f.subjects.findByClass(f.classId).size() == 2);
    CHECK_THROWS_AS(f.subjects.create(testing::makeSubject(f.classId, "Maths", "MATH")),
                    ConflictError);  // code deja utilise dans la classe
}

TEST_CASE("GradeRepository : enregistrement et agregation des moyennes par matiere") {
    Fixture f;
    auto s = testing::makeStudent("STU-2025-0001");
    s.classId = f.classId;
    const auto studentId = f.students.create(s);

    auto addGrade = [&](long long subjectId, double score, double maxScore, int term) {
        Grade g;
        g.studentId = studentId;
        g.subjectId = subjectId;
        g.evalType = EvalType::Exam;
        g.score = score;
        g.maxScore = maxScore;
        g.evalDate = "2025-10-10";
        g.term = term;
        return f.grades.create(g);
    };

    addGrade(f.mathId, 16.0, 20.0, 1);   // 16/20
    addGrade(f.mathId, 30.0, 50.0, 1);   // 12/20 apres mise a l'echelle
    addGrade(f.frenchId, 10.0, 20.0, 1);
    addGrade(f.mathId, 20.0, 20.0, 2);   // autre trimestre

    SUBCASE("les notes sont ramenees sur 20 avant moyenne") {
        const auto rows = f.grades.subjectAverages(studentId, 1);
        REQUIRE(rows.size() == 2);
        const auto& french = rows[0];  // tri alphabetique : Francais puis Mathematiques
        const auto& math = rows[1];
        CHECK(french.subjectName == "Francais");
        CHECK(math.subjectName == "Mathematiques");
        CHECK(math.average20 == doctest::Approx(14.0));
        CHECK(math.best20 == doctest::Approx(16.0));
        CHECK(math.worst20 == doctest::Approx(12.0));
        CHECK(math.gradeCount == 2);
        CHECK(math.coefficient == doctest::Approx(4.0));
    }

    SUBCASE("filtrage par trimestre") {
        const auto rows = f.grades.subjectAverages(studentId, 2);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].average20 == doctest::Approx(20.0));
    }

    SUBCASE("sans filtre de trimestre, toutes les notes sont agregees") {
        const auto rows = f.grades.subjectAverages(studentId, std::nullopt);
        REQUIRE(rows.size() == 2);
        CHECK(rows[1].gradeCount == 3);
    }

    SUBCASE("recherche filtree des notes") {
        GradeFilter filter;
        filter.studentId = studentId;
        CHECK(f.grades.find(filter).size() == 4);
        filter.subjectId = f.mathId;
        CHECK(f.grades.find(filter).size() == 3);
        filter.term = 1;
        CHECK(f.grades.find(filter).size() == 2);
    }

    SUBCASE("agregation a l'echelle de la classe") {
        CHECK(f.grades.classSubjectAverages(f.classId, 1).size() == 2);
    }

    SUBCASE("la suppression d'un eleve supprime ses notes (ON DELETE CASCADE)") {
        f.students.remove(studentId);
        GradeFilter filter;
        filter.studentId = studentId;
        CHECK(f.grades.find(filter).empty());
    }
}

TEST_CASE("AttendanceRepository : compteurs et taux de presence") {
    Fixture f;
    auto s = testing::makeStudent("STU-2025-0001");
    s.classId = f.classId;
    const auto studentId = f.students.create(s);

    auto mark = [&](const std::string& date, AttendanceStatus status) {
        Attendance a;
        a.studentId = studentId;
        a.date = date;
        a.time = std::string("08:00");
        a.status = status;
        return f.attendance.create(a);
    };

    mark("2025-10-01", AttendanceStatus::Present);
    mark("2025-10-02", AttendanceStatus::Absent);
    mark("2025-10-03", AttendanceStatus::Excused);
    mark("2025-10-04", AttendanceStatus::Late);
    mark("2025-11-05", AttendanceStatus::Absent);

    const auto summary = f.attendance.summaryForStudent(studentId);
    CHECK(summary.total == 5);
    CHECK(summary.present == 1);
    CHECK(summary.absent == 2);
    CHECK(summary.excused == 1);
    CHECK(summary.late == 1);
    CHECK(summary.attendanceRate == doctest::Approx(40.0));  // (1 present + 1 retard) / 5

    SUBCASE("filtrage par periode et statut") {
        AttendanceFilter filter;
        filter.studentId = studentId;
        filter.from = std::string("2025-10-02");
        filter.to = std::string("2025-10-04");
        CHECK(f.attendance.find(filter).size() == 3);
        filter.status = AttendanceStatus::Absent;
        CHECK(f.attendance.find(filter).size() == 1);
    }

    SUBCASE("absences agregees par mois") {
        const auto monthly = f.attendance.monthlyAbsences(f.classId);
        REQUIRE(monthly.size() == 2);
        CHECK(monthly[0].first == "2025-10");
        CHECK(monthly[0].second == 2);
        CHECK(monthly[1].first == "2025-11");
        CHECK(monthly[1].second == 1);
    }

    SUBCASE("doublon exact de releve -> conflit") {
        CHECK_THROWS_AS(mark("2025-10-01", AttendanceStatus::Absent), ConflictError);
    }

    SUBCASE("synthese par classe") {
        CHECK(f.attendance.summaryForClass(f.classId).size() == 1);
    }
}

TEST_CASE("UserRepository : CRUD et recherche par identifiants") {
    Fixture f;
    User user;
    user.username = "prof.kone";
    user.email = "kone@example.org";
    user.passwordHash = "pbkdf2_sha256$120000$sel$hash";
    user.fullName = "Mariam Kone";
    user.role = UserRole::Teacher;
    // Tout compte appartient a un etablissement : sans rattachement, il reste
    // invisible aux lectures, qui sont toutes cloisonnees.
    user.schoolId = app::tenant::currentSchool();

    const auto id = f.users.create(user);
    CHECK(f.users.count() == 1);

    auto found = f.users.findByUsername("prof.kone");
    REQUIRE(found.has_value());
    CHECK(toString(found->role) == "TEACHER");
    CHECK(found->isActive);
    CHECK(f.users.findByEmail("kone@example.org").has_value());
    CHECK_FALSE(f.users.findByUsername("inconnu").has_value());

    f.users.updatePassword(id, "pbkdf2_sha256$120000$sel2$hash2");
    CHECK(f.users.findById(id)->passwordHash == "pbkdf2_sha256$120000$sel2$hash2");

    CHECK_THROWS_AS(f.users.create(user), ConflictError);  // username deja pris

    CHECK(f.users.remove(id));
    CHECK(f.users.count() == 0);
}

TEST_CASE("Le hash de mot de passe n'est jamais serialise en JSON") {
    User user;
    user.username = "admin";
    user.email = "admin@example.org";
    user.passwordHash = "pbkdf2_sha256$120000$secret$ultrasecret";
    user.fullName = "Administrateur";
    user.role = UserRole::Admin;

    const auto json = user.toJson();
    CHECK_FALSE(json.contains("password_hash"));
    CHECK_FALSE(json.contains("password"));
    CHECK(json.dump().find("ultrasecret") == std::string::npos);
}

TEST_CASE("SchoolYearRepository : une seule annee courante a la fois") {
    Fixture f;
    auto second = testing::makeSchoolYear("2026-2027");
    second.startDate = "2026-09-01";
    second.endDate = "2027-07-15";
    const auto secondId = f.years.create(second);

    auto current = f.years.findCurrent();
    REQUIRE(current.has_value());
    CHECK(current->id == secondId);

    f.years.setCurrent(f.yearId);
    CHECK(f.years.findCurrent()->id == f.yearId);
    CHECK(f.years.findAll().size() == 2);
}
