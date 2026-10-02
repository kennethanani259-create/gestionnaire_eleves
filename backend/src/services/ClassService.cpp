#include "services/ClassService.hpp"

#include <algorithm>
#include <cctype>

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "utils/DateTime.hpp"
#include "utils/Validator.hpp"

namespace app {
namespace {

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string upper(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) out.push_back(static_cast<char>(::toupper(static_cast<unsigned char>(c))));
    return out;
}

/// Deduit le niveau du nom de la classe ("Terminale C" -> "Terminale").
std::string deduceLevel(const std::string& name) {
    const auto space = name.find(' ');
    return space == std::string::npos ? name : name.substr(0, space);
}

}  // namespace

// ------------------------------------------------------------------ Classes

void ClassService::validateClass(const ClassRoom& value) {
    Validator validator;
    validator.required("name", value.name)
        .maxLength("name", value.name, 60)
        .required("level", value.level)
        .maxLength("level", value.level, 40);

    if (value.capacity.has_value() && *value.capacity <= 0) {
        validator.add("capacity", "La capacite doit etre strictement positive");
    }
    if (!years_.findById(value.schoolYearId).has_value()) {
        validator.add("school_year_id", "L'annee scolaire indiquee n'existe pas");
    }
    if (value.mainTeacherId.has_value() && !teachers_.exists(*value.mainTeacherId)) {
        validator.add("main_teacher_id", "L'enseignant principal indique n'existe pas");
    }
    validator.throwIfInvalid();
}

ClassRoom ClassService::create(ClassRoom input) {
    input.name = trim(input.name);
    input.level = trim(input.level.empty() ? deduceLevel(input.name) : input.level);
    if (input.schoolYearId == 0) {
        input.schoolYearId = currentYear().id;  // leve NotFoundError si aucune annee
    }
    validateClass(input);
    const long long id = classes_.create(input);
    LOG_INFO("class.service", "Classe creee: " + input.name);
    return get(id);
}

ClassRoom ClassService::update(long long id, ClassRoom input) {
    auto existing = classes_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Classe", id);
    input.id = id;
    input.name = trim(input.name);
    input.level = trim(input.level.empty() ? deduceLevel(input.name) : input.level);
    if (input.schoolYearId == 0) input.schoolYearId = existing->schoolYearId;
    validateClass(input);
    classes_.update(input);
    return get(id);
}

void ClassService::remove(long long id) {
    auto existing = classes_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Classe", id);
    if (existing->studentCount > 0) {
        throw ConflictError("Impossible de supprimer la classe: " +
                            std::to_string(existing->studentCount) +
                            " eleve(s) y sont encore inscrits");
    }
    classes_.remove(id);
    LOG_WARN("class.service", "Classe supprimee: id=" + std::to_string(id));
}

ClassRoom ClassService::get(long long id) {
    auto value = classes_.findById(id);
    if (!value.has_value()) throw NotFoundError("Classe", id);
    return *value;
}

std::vector<ClassRoom> ClassService::list(const ClassFilter& filter) {
    return classes_.findAll(filter);
}

// ----------------------------------------------------------------- Matieres

void ClassService::validateSubject(const Subject& value) {
    Validator validator;
    validator.required("name", value.name)
        .maxLength("name", value.name, 80)
        .required("code", value.code)
        .maxLength("code", value.code, 16)
        .range("coefficient", value.coefficient, 0.5, 20.0);

    if (!classes_.exists(value.classId)) {
        validator.add("class_id", "La classe indiquee n'existe pas");
    }
    if (value.teacherId.has_value() && !teachers_.exists(*value.teacherId)) {
        validator.add("teacher_id", "L'enseignant indique n'existe pas");
    }
    validator.throwIfInvalid();
}

Subject ClassService::createSubject(Subject input) {
    input.name = trim(input.name);
    input.code = upper(trim(input.code));
    validateSubject(input);
    const long long id = subjects_.create(input);
    return getSubject(id);
}

Subject ClassService::updateSubject(long long id, Subject input) {
    auto existing = subjects_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Matiere", id);
    input.id = id;
    input.name = trim(input.name);
    input.code = upper(trim(input.code));
    if (input.classId == 0) input.classId = existing->classId;
    validateSubject(input);
    subjects_.update(input);
    return getSubject(id);
}

void ClassService::removeSubject(long long id) {
    if (!subjects_.remove(id)) throw NotFoundError("Matiere", id);
    LOG_WARN("class.service", "Matiere supprimee (notes associees comprises): id=" +
                                  std::to_string(id));
}

Subject ClassService::getSubject(long long id) {
    auto value = subjects_.findById(id);
    if (!value.has_value()) throw NotFoundError("Matiere", id);
    return *value;
}

std::vector<Subject> ClassService::subjectsOfClass(long long classId) {
    if (!classes_.exists(classId)) throw NotFoundError("Classe", classId);
    return subjects_.findByClass(classId);
}

std::vector<Subject> ClassService::allSubjects() { return subjects_.findAll(); }

// -------------------------------------------------------------- Enseignants

void ClassService::validateTeacher(const Teacher& value) {
    Validator validator;
    validator.required("first_name", value.firstName)
        .maxLength("first_name", value.firstName, 80)
        .required("last_name", value.lastName)
        .maxLength("last_name", value.lastName, 80)
        .optionalEmail("email", value.email)
        .phone("phone", value.phone);
    validator.throwIfInvalid();
}

Teacher ClassService::createTeacher(Teacher input) {
    input.firstName = trim(input.firstName);
    input.lastName = trim(input.lastName);
    validateTeacher(input);
    const long long id = teachers_.create(input);
    auto created = teachers_.findById(id);
    if (!created.has_value()) throw NotFoundError("Enseignant", id);
    return *created;
}

Teacher ClassService::updateTeacher(long long id, Teacher input) {
    if (!teachers_.exists(id)) throw NotFoundError("Enseignant", id);
    input.id = id;
    input.firstName = trim(input.firstName);
    input.lastName = trim(input.lastName);
    validateTeacher(input);
    teachers_.update(input);
    return *teachers_.findById(id);
}

void ClassService::removeTeacher(long long id) {
    if (!teachers_.remove(id)) throw NotFoundError("Enseignant", id);
}

std::vector<Teacher> ClassService::allTeachers() { return teachers_.findAll(); }

// ---------------------------------------------------------- Annees scolaires

SchoolYear ClassService::createYear(SchoolYear input) {
    Validator validator;
    validator.required("label", input.label)
        .date("start_date", input.startDate)
        .date("end_date", input.endDate);
    if (datetime::isValidDate(input.startDate) && datetime::isValidDate(input.endDate) &&
        input.endDate <= input.startDate) {
        validator.add("end_date", "La date de fin doit suivre la date de debut");
    }
    validator.throwIfInvalid();

    const long long id = years_.create(input);
    auto created = years_.findById(id);
    if (!created.has_value()) throw NotFoundError("Annee scolaire", id);
    return *created;
}

std::vector<SchoolYear> ClassService::allYears() { return years_.findAll(); }

SchoolYear ClassService::currentYear() {
    auto year = years_.findCurrent();
    if (!year.has_value()) {
        throw NotFoundError(
            "Aucune annee scolaire courante definie. Creez-en une avant de continuer.");
    }
    return *year;
}

}  // namespace app
