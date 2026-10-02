#include "services/StudentService.hpp"

#include <algorithm>

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

/// Normalise un optionnel : une chaine vide devient absente (NULL en base).
std::optional<std::string> normalize(const std::optional<std::string>& value) {
    if (!value.has_value()) return std::nullopt;
    const std::string trimmed = trim(*value);
    if (trimmed.empty()) return std::nullopt;
    return trimmed;
}

}  // namespace

nlohmann::json StudentPage::toJson() const {
    nlohmann::json items_json = nlohmann::json::array();
    for (const auto& item : items) items_json.push_back(item.toJson());
    return {{"items", items_json},
            {"total", total},
            {"limit", limit},
            {"offset", offset},
            {"count", static_cast<long long>(items.size())}};
}

void StudentService::validate(const Student& student, std::optional<long long> existingId) {
    Validator validator;
    validator.required("first_name", student.firstName)
        .maxLength("first_name", student.firstName, 80)
        .required("last_name", student.lastName)
        .maxLength("last_name", student.lastName, 80)
        .pastDate("birth_date", student.birthDate)
        .date("enrollment_date", student.enrollmentDate)
        .optionalEmail("email", student.email)
        .phone("phone", student.phone)
        .phone("guardian_phone", student.guardianPhone)
        .maxLength("address", student.address.value_or(""), 255)
        .maxLength("guardian_name", student.guardianName.value_or(""), 120);

    if (datetime::isValidDate(student.birthDate)) {
        const int age = datetime::ageFromBirthDate(student.birthDate);
        if (age < 2 || age > 100) {
            validator.add("birth_date", "L'age doit etre compris entre 2 et 100 ans");
        }
    }
    if (datetime::isValidDate(student.birthDate) &&
        datetime::isValidDate(student.enrollmentDate) &&
        student.enrollmentDate < student.birthDate) {
        validator.add("enrollment_date",
                      "La date d'inscription doit suivre la date de naissance");
    }
    if (student.matricule.empty()) {
        validator.add("matricule", "Le matricule ne peut pas etre vide");
    } else if (students_.matriculeExists(student.matricule, existingId)) {
        validator.add("matricule", "Ce matricule est deja attribue");
    }
    if (student.classId.has_value() && !classes_.exists(*student.classId)) {
        validator.add("class_id", "La classe indiquee n'existe pas");
    }
    validator.throwIfInvalid();
}

Student StudentService::create(Student input) {
    input.firstName = trim(input.firstName);
    input.lastName = trim(input.lastName);
    input.matricule = trim(input.matricule);
    input.address = normalize(input.address);
    input.phone = normalize(input.phone);
    input.email = normalize(input.email);
    input.guardianName = normalize(input.guardianName);
    input.guardianPhone = normalize(input.guardianPhone);
    input.photoPath = normalize(input.photoPath);
    if (input.enrollmentDate.empty()) input.enrollmentDate = datetime::today();
    if (input.matricule.empty()) {
        input.matricule = students_.nextMatricule(datetime::currentYear());
    }

    validate(input, std::nullopt);
    const long long id = students_.create(input);
    LOG_INFO("student.service", "Eleve cree: " + input.matricule + " (id=" + std::to_string(id) + ")");
    return get(id);
}

Student StudentService::update(long long id, Student input) {
    auto existing = students_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Eleve", id);

    input.id = id;
    input.firstName = trim(input.firstName);
    input.lastName = trim(input.lastName);
    input.matricule = trim(input.matricule.empty() ? existing->matricule : input.matricule);
    input.address = normalize(input.address);
    input.phone = normalize(input.phone);
    input.email = normalize(input.email);
    input.guardianName = normalize(input.guardianName);
    input.guardianPhone = normalize(input.guardianPhone);
    input.photoPath = normalize(input.photoPath);
    if (input.enrollmentDate.empty()) input.enrollmentDate = existing->enrollmentDate;

    validate(input, id);
    students_.update(input);
    LOG_INFO("student.service", "Eleve modifie: id=" + std::to_string(id));
    return get(id);
}

void StudentService::remove(long long id) {
    if (!students_.remove(id)) throw NotFoundError("Eleve", id);
    LOG_WARN("student.service", "Eleve supprime definitivement: id=" + std::to_string(id));
}

Student StudentService::get(long long id) {
    auto student = students_.findById(id);
    if (!student.has_value()) throw NotFoundError("Eleve", id);
    return *student;
}

StudentPage StudentService::list(const StudentFilter& filter) {
    StudentFilter sanitized = filter;
    sanitized.limit = std::clamp(filter.limit, 0, 500);  // protege contre les requetes massives
    sanitized.offset = std::max(0, filter.offset);

    StudentPage page;
    page.items = students_.search(sanitized);
    page.total = students_.count(sanitized);
    page.limit = sanitized.limit;
    page.offset = sanitized.offset;
    return page;
}

std::vector<Student> StudentService::byClass(long long classId, bool activeOnly) {
    if (!classes_.exists(classId)) throw NotFoundError("Classe", classId);
    return students_.findByClass(classId, activeOnly);
}

long long StudentService::countAll() { return students_.count(StudentFilter{}); }

}  // namespace app
