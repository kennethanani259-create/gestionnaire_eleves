/// Tests des services metier : regles, calculs de moyennes, classement, presences.
#include <doctest/doctest.h>

#include "core/Error.hpp"
#include "services/AttendanceService.hpp"
#include "services/ClassService.hpp"
#include "services/GradeService.hpp"
#include "services/StudentService.hpp"
#include "TestHelpers.hpp"

using namespace app;

namespace {

/// Contexte complet : repositories + services cables comme en production.
struct ServiceFixture {
    std::unique_ptr<Database> db = testing::makeTestDatabase();
    SchoolYearRepository yearRepo{*db};
    ClassRepository classRepo{*db};
    SubjectRepository subjectRepo{*db};
    StudentRepository studentRepo{*db};
    GradeRepository gradeRepo{*db};
    AttendanceRepository attendanceRepo{*db};
    TeacherRepository teacherRepo{*db};

    StudentService studentService{studentRepo, classRepo};
    ClassService classService{classRepo, subjectRepo, studentRepo, teacherRepo, yearRepo};
    GradeService gradeService{gradeRepo, studentRepo, subjectRepo, classRepo, attendanceRepo};
    AttendanceService attendanceService{attendanceRepo, studentRepo, subjectRepo, classRepo};

    long long yearId = 0;
    long long classId = 0;
    long long mathId = 0;
    long long frenchId = 0;

    ServiceFixture() {
        yearId = yearRepo.create(testing::makeSchoolYear());
        classId = classRepo.create(testing::makeClass(yearId));
        mathId = subjectRepo.create(testing::makeSubject(classId, "Mathematiques", "MATH", 4.0));
        frenchId = subjectRepo.create(testing::makeSubject(classId, "Francais", "FR", 1.0));
    }

    long long addStudent(const std::string& firstName, const std::string& lastName) {
        Student student;
        student.firstName = firstName;
        student.lastName = lastName;
        student.birthDate = "2012-04-18";
        student.gender = Gender::Female;
        student.classId = classId;
        return studentService.create(student).id;  // matricule genere automatiquement
    }

    void addGrade(long long studentId, long long subjectId, double score, double maxScore,
                  int term = 1) {
        Grade grade;
        grade.studentId = studentId;
        grade.subjectId = subjectId;
        grade.evalType = EvalType::Exam;
        grade.score = score;
        grade.maxScore = maxScore;
        grade.evalDate = "2025-10-10";
        grade.term = term;
        gradeService.create(grade);
    }
};

}  // namespace

// ===========================================================================
// StudentService
// ===========================================================================

TEST_CASE("StudentService : creation avec matricule et date d'inscription automatiques") {
    ServiceFixture f;
    Student student;
    student.firstName = "  Awa  ";  // les espaces superflus sont nettoyes
    student.lastName = "Dossou";
    student.birthDate = "2012-04-18";
    student.classId = f.classId;
    student.email = std::string("   ");  // chaine vide -> NULL

    const auto created = f.studentService.create(student);
    CHECK(created.firstName == "Awa");
    CHECK(created.matricule.rfind("STU-", 0) == 0);
    CHECK_FALSE(created.enrollmentDate.empty());
    CHECK_FALSE(created.email.has_value());
    CHECK(created.className == std::string("6e A"));
}

TEST_CASE("StudentService : la validation refuse les donnees incoherentes") {
    ServiceFixture f;

    SUBCASE("nom vide") {
        Student student;
        student.lastName = "Dossou";
        student.birthDate = "2012-04-18";
        CHECK_THROWS_AS(f.studentService.create(student), ValidationError);
    }

    SUBCASE("date de naissance invalide") {
        Student student;
        student.firstName = "Awa";
        student.lastName = "Dossou";
        student.birthDate = "2012-02-30";
        CHECK_THROWS_AS(f.studentService.create(student), ValidationError);
    }

    SUBCASE("date de naissance dans le futur") {
        Student student;
        student.firstName = "Awa";
        student.lastName = "Dossou";
        student.birthDate = "2099-01-01";
        CHECK_THROWS_AS(f.studentService.create(student), ValidationError);
    }

    SUBCASE("e-mail mal forme") {
        Student student;
        student.firstName = "Awa";
        student.lastName = "Dossou";
        student.birthDate = "2012-04-18";
        student.email = std::string("pas-un-email");
        CHECK_THROWS_AS(f.studentService.create(student), ValidationError);
    }

    SUBCASE("classe inexistante") {
        Student student;
        student.firstName = "Awa";
        student.lastName = "Dossou";
        student.birthDate = "2012-04-18";
        student.classId = 9999;
        CHECK_THROWS_AS(f.studentService.create(student), ValidationError);
    }

    SUBCASE("le detail indique precisement les champs fautifs") {
        Student student;
        student.birthDate = "pas-une-date";
        try {
            f.studentService.create(student);
            FAIL("une exception etait attendue");
        } catch (const ValidationError& e) {
            const auto fields = e.details()["fields"];
            CHECK(fields.contains("first_name"));
            CHECK(fields.contains("last_name"));
            CHECK(fields.contains("birth_date"));
        }
    }
}

TEST_CASE("StudentService : modification, suppression et pagination") {
    ServiceFixture f;
    const auto id = f.addStudent("Awa", "Dossou");
    f.addStudent("Kodjo", "Houngbo");

    auto student = f.studentService.get(id);
    student.lastName = "Dossou-Kone";
    const auto updated = f.studentService.update(id, student);
    CHECK(updated.lastName == "Dossou-Kone");
    CHECK(updated.matricule == student.matricule);  // matricule conserve

    StudentFilter filter;
    filter.limit = 1;
    const auto page = f.studentService.list(filter);
    CHECK(page.items.size() == 1);
    CHECK(page.total == 2);
    CHECK(page.toJson()["total"] == 2);

    f.studentService.remove(id);
    CHECK_THROWS_AS(f.studentService.get(id), NotFoundError);
    CHECK_THROWS_AS(f.studentService.remove(id), NotFoundError);
}

TEST_CASE("StudentService : la limite de pagination est bornee") {
    ServiceFixture f;
    StudentFilter filter;
    filter.limit = 100000;  // tentative de requete massive
    CHECK(f.studentService.list(filter).limit == 500);
}

// ===========================================================================
// GradeService : calculs
// ===========================================================================

TEST_CASE("GradeService : la moyenne generale est ponderee par les coefficients") {
    ServiceFixture f;
    const auto studentId = f.addStudent("Awa", "Dossou");
    f.addGrade(studentId, f.mathId, 15.0, 20.0);    // coefficient 4
    f.addGrade(studentId, f.frenchId, 10.0, 20.0);  // coefficient 1

    const auto result = f.gradeService.studentResult(studentId, std::nullopt);
    // (15*4 + 10*1) / 5 = 14
    CHECK(result.generalAverage == doctest::Approx(14.0));
    CHECK(result.totalCoefficients == doctest::Approx(5.0));
    CHECK(result.appreciation == "Tres bien");
    CHECK(result.subjects.size() == 2);
    CHECK(result.gradeCount == 2);
    CHECK(result.bestScore == doctest::Approx(15.0));
    CHECK(result.worstScore == doctest::Approx(10.0));
}

TEST_CASE("GradeService : les notes sont ramenees sur 20 quel que soit le bareme") {
    ServiceFixture f;
    const auto studentId = f.addStudent("Awa", "Dossou");
    f.addGrade(studentId, f.mathId, 45.0, 50.0);   // 18/20
    f.addGrade(studentId, f.mathId, 7.0, 10.0);    // 14/20

    const auto result = f.gradeService.studentResult(studentId, std::nullopt);
    CHECK(result.subjects.front().average == doctest::Approx(16.0));
    CHECK(result.subjects.front().best == doctest::Approx(18.0));
    CHECK(result.subjects.front().worst == doctest::Approx(14.0));
}

TEST_CASE("GradeService : filtrage des resultats par trimestre") {
    ServiceFixture f;
    const auto studentId = f.addStudent("Awa", "Dossou");
    f.addGrade(studentId, f.mathId, 8.0, 20.0, 1);
    f.addGrade(studentId, f.mathId, 18.0, 20.0, 2);

    CHECK(f.gradeService.studentResult(studentId, 1).generalAverage == doctest::Approx(8.0));
    CHECK(f.gradeService.studentResult(studentId, 2).generalAverage == doctest::Approx(18.0));
    CHECK(f.gradeService.studentResult(studentId, std::nullopt).generalAverage ==
          doctest::Approx(13.0));
}

TEST_CASE("GradeService : appreciations selon la moyenne") {
    CHECK(GradeService::appreciationFor(18.0) == "Excellent");
    CHECK(GradeService::appreciationFor(16.0) == "Excellent");
    CHECK(GradeService::appreciationFor(14.5) == "Tres bien");
    CHECK(GradeService::appreciationFor(12.0) == "Bien");
    CHECK(GradeService::appreciationFor(10.0) == "Assez bien");
    CHECK(GradeService::appreciationFor(9.99) == "Insuffisant");
    CHECK(GradeService::appreciationFor(5.0) == "Tres insuffisant");
}

TEST_CASE("GradeService : classement de la classe en rangs de competition") {
    ServiceFixture f;
    const auto first = f.addStudent("Awa", "Dossou");      // 18
    const auto second = f.addStudent("Bintou", "Adjovi");  // 15
    const auto third = f.addStudent("Kodjo", "Houngbo");   // 15 (ex aequo)
    const auto fourth = f.addStudent("Moussa", "Zinsou");  // 9

    f.addGrade(first, f.mathId, 18.0, 20.0);
    f.addGrade(second, f.mathId, 15.0, 20.0);
    f.addGrade(third, f.mathId, 15.0, 20.0);
    f.addGrade(fourth, f.mathId, 9.0, 20.0);

    const auto ranking = f.gradeService.classRanking(f.classId, std::nullopt);
    REQUIRE(ranking.size() == 4);
    CHECK(ranking[0].studentId == first);
    CHECK(ranking[0].rank == 1);
    CHECK(ranking[1].rank == 2);
    CHECK(ranking[2].rank == 2);  // ex aequo : meme rang
    CHECK(ranking[3].rank == 4);  // le rang 3 est saute
    CHECK(ranking[3].studentId == fourth);

    // Le rang est coherent dans le bilan individuel.
    CHECK(f.gradeService.studentResult(third, std::nullopt).rank == 2);
    CHECK(f.gradeService.studentResult(first, std::nullopt).classSize == 4);
}

TEST_CASE("GradeService : un eleve sans note n'est pas classe") {
    ServiceFixture f;
    const auto noted = f.addStudent("Awa", "Dossou");
    const auto unnoted = f.addStudent("Kodjo", "Houngbo");
    f.addGrade(noted, f.mathId, 12.0, 20.0);

    const auto result = f.gradeService.studentResult(unnoted, std::nullopt);
    CHECK(result.rank == 0);
    CHECK(result.generalAverage == doctest::Approx(0.0));
    CHECK(result.appreciation == "Aucune note enregistree");
}

TEST_CASE("GradeService : validation des notes") {
    ServiceFixture f;
    const auto studentId = f.addStudent("Awa", "Dossou");

    auto makeGrade = [&](double score, double maxScore) {
        Grade grade;
        grade.studentId = studentId;
        grade.subjectId = f.mathId;
        grade.score = score;
        grade.maxScore = maxScore;
        grade.evalDate = "2025-10-10";
        return grade;
    };

    SUBCASE("note negative refusee") {
        CHECK_THROWS_AS(f.gradeService.create(makeGrade(-1.0, 20.0)), ValidationError);
    }
    SUBCASE("note superieure au bareme refusee") {
        CHECK_THROWS_AS(f.gradeService.create(makeGrade(21.0, 20.0)), ValidationError);
    }
    SUBCASE("bareme nul refuse") {
        CHECK_THROWS_AS(f.gradeService.create(makeGrade(10.0, 0.0)), ValidationError);
    }
    SUBCASE("date d'evaluation future refusee") {
        auto grade = makeGrade(10.0, 20.0);
        grade.evalDate = "2099-01-01";
        CHECK_THROWS_AS(f.gradeService.create(grade), ValidationError);
    }
    SUBCASE("trimestre hors bornes refuse") {
        auto grade = makeGrade(10.0, 20.0);
        grade.term = 5;
        CHECK_THROWS_AS(f.gradeService.create(grade), ValidationError);
    }
    SUBCASE("matiere inexistante refusee") {
        auto grade = makeGrade(10.0, 20.0);
        grade.subjectId = 9999;
        CHECK_THROWS_AS(f.gradeService.create(grade), ValidationError);
    }
    SUBCASE("matiere d'une autre classe refusee") {
        const auto otherClassId = f.classRepo.create(testing::makeClass(f.yearId, "5e B"));
        const auto otherSubjectId =
            f.subjectRepo.create(testing::makeSubject(otherClassId, "Anglais", "ANG", 2.0));
        auto grade = makeGrade(10.0, 20.0);
        grade.subjectId = otherSubjectId;
        CHECK_THROWS_AS(f.gradeService.create(grade), ValidationError);
    }
    SUBCASE("note valide acceptee aux bornes") {
        CHECK_NOTHROW(f.gradeService.create(makeGrade(0.0, 20.0)));
        CHECK_NOTHROW(f.gradeService.create(makeGrade(20.0, 20.0)));
    }
}

TEST_CASE("GradeService : statistiques par matiere d'une classe") {
    ServiceFixture f;
    const auto a = f.addStudent("Awa", "Dossou");
    const auto b = f.addStudent("Kodjo", "Houngbo");
    f.addGrade(a, f.mathId, 16.0, 20.0);
    f.addGrade(b, f.mathId, 10.0, 20.0);

    const auto stats = f.gradeService.classSubjectStatistics(f.classId, std::nullopt);
    REQUIRE(stats.size() == 1);
    CHECK(stats[0]["subject_name"] == "Mathematiques");
    CHECK(stats[0]["average"].get<double>() == doctest::Approx(13.0));
    CHECK(stats[0]["student_count"] == 2);
}

// ===========================================================================
// ClassService
// ===========================================================================

TEST_CASE("ClassService : le niveau est deduit du nom et la classe est validee") {
    ServiceFixture f;
    ClassRoom input;
    input.name = "Terminale C";
    input.schoolYearId = f.yearId;
    const auto created = f.classService.create(input);
    CHECK(created.level == "Terminale");

    SUBCASE("nom vide refuse") {
        ClassRoom invalid;
        invalid.schoolYearId = f.yearId;
        CHECK_THROWS_AS(f.classService.create(invalid), ValidationError);
    }
    SUBCASE("annee scolaire inexistante refusee") {
        ClassRoom invalid;
        invalid.name = "3e A";
        invalid.schoolYearId = 9999;
        CHECK_THROWS_AS(f.classService.create(invalid), ValidationError);
    }
}

TEST_CASE("ClassService : suppression refusee si la classe contient des eleves") {
    ServiceFixture f;
    f.addStudent("Awa", "Dossou");
    CHECK_THROWS_AS(f.classService.remove(f.classId), ConflictError);

    const auto emptyClassId = f.classService.create([&] {
                                   ClassRoom c;
                                   c.name = "3e B";
                                   c.schoolYearId = f.yearId;
                                   return c;
                               }()).id;
    CHECK_NOTHROW(f.classService.remove(emptyClassId));
}

TEST_CASE("ClassService : matieres, coefficients et code normalise") {
    ServiceFixture f;
    Subject subject;
    subject.name = "Anglais";
    subject.code = "ang";  // sera normalise en majuscules
    subject.coefficient = 2.0;
    subject.classId = f.classId;
    const auto created = f.classService.createSubject(subject);
    CHECK(created.code == "ANG");

    SUBCASE("coefficient hors bornes refuse") {
        subject.code = "ANG2";
        subject.coefficient = 50.0;
        CHECK_THROWS_AS(f.classService.createSubject(subject), ValidationError);
    }
    SUBCASE("classe inexistante refusee") {
        subject.code = "ANG3";
        subject.classId = 9999;
        CHECK_THROWS_AS(f.classService.createSubject(subject), ValidationError);
    }
    SUBCASE("liste des matieres de la classe") {
        CHECK(f.classService.subjectsOfClass(f.classId).size() == 3);
    }
}

TEST_CASE("ClassService : annee scolaire courante") {
    ServiceFixture f;
    CHECK(f.classService.currentYear().label == "2025-2026");

    SchoolYear invalid;
    invalid.label = "2027-2026";
    invalid.startDate = "2027-09-01";
    invalid.endDate = "2026-07-15";  // fin avant debut
    CHECK_THROWS_AS(f.classService.createYear(invalid), ValidationError);
}

// ===========================================================================
// AttendanceService
// ===========================================================================

TEST_CASE("AttendanceService : saisie, justification obligatoire et statistiques") {
    ServiceFixture f;
    const auto studentId = f.addStudent("Awa", "Dossou");

    Attendance entry;
    entry.studentId = studentId;
    entry.date = "2025-10-01";
    entry.status = AttendanceStatus::Absent;
    CHECK_NOTHROW(f.attendanceService.create(entry));

    SUBCASE("une absence justifiee sans justification est refusee") {
        Attendance excused;
        excused.studentId = studentId;
        excused.date = "2025-10-02";
        excused.status = AttendanceStatus::Excused;
        CHECK_THROWS_AS(f.attendanceService.create(excused), ValidationError);

        excused.justification = std::string("Certificat medical");
        CHECK_NOTHROW(f.attendanceService.create(excused));
    }

    SUBCASE("date future refusee") {
        Attendance future;
        future.studentId = studentId;
        future.date = "2099-01-01";
        future.status = AttendanceStatus::Present;
        CHECK_THROWS_AS(f.attendanceService.create(future), ValidationError);
    }

    SUBCASE("heure invalide refusee") {
        Attendance bad;
        bad.studentId = studentId;
        bad.date = "2025-10-03";
        bad.time = std::string("25:00");
        bad.status = AttendanceStatus::Present;
        CHECK_THROWS_AS(f.attendanceService.create(bad), ValidationError);
    }

    SUBCASE("eleve inexistant refuse") {
        Attendance ghost;
        ghost.studentId = 9999;
        ghost.date = "2025-10-03";
        ghost.status = AttendanceStatus::Present;
        CHECK_THROWS_AS(f.attendanceService.create(ghost), ValidationError);
    }

    SUBCASE("synthese individuelle") {
        const auto summary = f.attendanceService.summary(studentId);
        CHECK(summary.total == 1);
        CHECK(summary.absent == 1);
        CHECK(summary.attendanceRate == doctest::Approx(0.0));
    }
}

TEST_CASE("AttendanceService : appel groupe d'une classe") {
    ServiceFixture f;
    const auto a = f.addStudent("Awa", "Dossou");
    const auto b = f.addStudent("Kodjo", "Houngbo");

    const int created = f.attendanceService.markClass(
        f.classId, "2025-10-06",
        {{a, AttendanceStatus::Present}, {b, AttendanceStatus::Late}}, std::string("08:00"));
    CHECK(created == 2);

    const auto stats = f.attendanceService.classStatistics(f.classId);
    CHECK(stats["total"] == 2);
    CHECK(stats["late"] == 1);
    CHECK(stats["attendance_rate"].get<double>() == doctest::Approx(100.0));

    SUBCASE("un eleve etranger a la classe est refuse") {
        const auto otherClassId = f.classRepo.create(testing::makeClass(f.yearId, "5e B"));
        CHECK_THROWS_AS(f.attendanceService.markClass(otherClassId, "2025-10-07",
                                                      {{a, AttendanceStatus::Present}},
                                                      std::nullopt),
                        ValidationError);
    }
    SUBCASE("liste vide refusee") {
        CHECK_THROWS_AS(f.attendanceService.markClass(f.classId, "2025-10-08", {}, std::nullopt),
                        ValidationError);
    }
}

TEST_CASE("Le bilan d'un eleve agrege notes et presences") {
    ServiceFixture f;
    const auto studentId = f.addStudent("Awa", "Dossou");
    f.addGrade(studentId, f.mathId, 16.0, 20.0);

    Attendance absence;
    absence.studentId = studentId;
    absence.date = "2025-10-01";
    absence.status = AttendanceStatus::Absent;
    f.attendanceService.create(absence);

    const auto result = f.gradeService.studentResult(studentId, std::nullopt);
    const auto json = result.toJson();
    CHECK(json["general_average"].get<double>() == doctest::Approx(16.0));
    CHECK(json["rank"] == 1);
    CHECK(json["attendance"]["absent"] == 1);
    CHECK(json["appreciation"] == "Excellent");
    CHECK(json["student"]["full_name"] == "Awa Dossou");
}
