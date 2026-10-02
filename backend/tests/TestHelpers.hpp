#pragma once
/**
 * @file TestHelpers.hpp
 * @brief Outils partages par les tests : base SQLite temporaire migree,
 *        et fabriques d'entites valides.
 */
#include <filesystem>
#include <string>

#include "database/Database.hpp"
#include "database/Migrator.hpp"
#include "models/ClassRoom.hpp"
#include "models/SchoolYear.hpp"
#include "models/Student.hpp"
#include "models/Subject.hpp"
#include "repositories/ClassRepository.hpp"
#include "repositories/SchoolYearRepository.hpp"
#include "repositories/StudentRepository.hpp"
#include "repositories/SubjectRepository.hpp"
#include "core/Tenant.hpp"
#include "repositories/SchoolRepository.hpp"
#include "repositories/UserRepository.hpp"

namespace testing {

/// Localise le repertoire des migrations quel que soit le repertoire courant.
inline std::string migrationsDir() {
    const char* candidates[] = {"migrations", "../migrations", "../../migrations",
                                "backend/migrations", "../backend/migrations"};
    for (const char* candidate : candidates) {
        if (std::filesystem::exists(std::filesystem::path(candidate) / "001_init.sql")) {
            return candidate;
        }
    }
    throw std::runtime_error("Repertoire de migrations introuvable depuis les tests");
}

/// Cree un etablissement et pose la portee courante dessus. Les depots
/// exigent une portee : sans elle, aucune ecriture n'est possible, ce qui est
/// exactement le comportement attendu en production.
inline long long seedSchool(app::Database& db, const std::string& name = "Ecole de test",
                            const std::string& code = "TEST01") {
    app::SchoolRepository schools(db);
    app::School school;
    school.code = code;
    school.name = name;
    const long long id = schools.create(school);
    app::tenant::setCurrentSchool(id);
    return id;
}

/// Base en memoire, schema applique, un etablissement courant : chaque test
/// part d'un etat propre et deja cloisonne.
inline std::unique_ptr<app::Database> makeTestDatabase() {
    auto db = std::make_unique<app::Database>(":memory:");
    app::Migrator migrator(*db, migrationsDir());
    migrator.migrate();
    seedSchool(*db);
    return db;
}

inline app::SchoolYear makeSchoolYear(const std::string& label = "2025-2026") {
    app::SchoolYear year;
    year.label = label;
    year.startDate = "2025-09-01";
    year.endDate = "2026-07-15";
    year.isCurrent = true;
    return year;
}

/// Le niveau est deduit du nom ("5e B" -> "5e"), comme dans la saisie reelle.
inline app::ClassRoom makeClass(long long schoolYearId, const std::string& name = "6e A") {
    app::ClassRoom value;
    value.name = name;
    value.level = name.substr(0, name.find(' '));
    value.schoolYearId = schoolYearId;
    return value;
}

inline app::Subject makeSubject(long long classId, const std::string& name = "Mathematiques",
                                const std::string& code = "MATH", double coefficient = 4.0) {
    app::Subject subject;
    subject.name = name;
    subject.code = code;
    subject.coefficient = coefficient;
    subject.classId = classId;
    return subject;
}

inline app::Student makeStudent(const std::string& matricule = "STU-2025-0001",
                                const std::string& firstName = "Awa",
                                const std::string& lastName = "Dossou") {
    app::Student student;
    student.matricule = matricule;
    student.firstName = firstName;
    student.lastName = lastName;
    student.birthDate = "2012-04-18";
    student.gender = app::Gender::Female;
    student.enrollmentDate = "2025-09-05";
    student.status = app::StudentStatus::Active;
    return student;
}

}  // namespace testing

