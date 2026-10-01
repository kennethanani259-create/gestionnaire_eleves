#include "services/AttendanceService.hpp"

#include <algorithm>

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "utils/DateTime.hpp"
#include "utils/Validator.hpp"

namespace app {

void AttendanceService::validate(const Attendance& value) {
    Validator validator;
    validator.date("date", value.date).time("time", value.time);

    if (datetime::isValidDate(value.date) && value.date > datetime::today()) {
        validator.add("date", "La date ne peut pas etre dans le futur");
    }
    if (!students_.findById(value.studentId).has_value()) {
        validator.add("student_id", "L'eleve indique n'existe pas");
    }
    if (value.subjectId.has_value() && !subjects_.exists(*value.subjectId)) {
        validator.add("subject_id", "La matiere indiquee n'existe pas");
    }
    // Une absence justifiee exige une justification.
    if (value.status == AttendanceStatus::Excused &&
        (!value.justification.has_value() || value.justification->empty())) {
        validator.add("justification",
                      "Une justification est obligatoire pour une absence justifiee");
    }
    validator.throwIfInvalid();
}

Attendance AttendanceService::create(Attendance input) {
    if (input.date.empty()) input.date = datetime::today();
    validate(input);
    const long long id = attendance_.create(input);
    return get(id);
}

Attendance AttendanceService::update(long long id, Attendance input) {
    auto existing = attendance_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Releve de presence", id);
    input.id = id;
    if (input.date.empty()) input.date = existing->date;
    validate(input);
    attendance_.update(input);
    return get(id);
}

void AttendanceService::remove(long long id) {
    if (!attendance_.remove(id)) throw NotFoundError("Releve de presence", id);
}

Attendance AttendanceService::get(long long id) {
    auto value = attendance_.findById(id);
    if (!value.has_value()) throw NotFoundError("Releve de presence", id);
    return *value;
}

std::vector<Attendance> AttendanceService::list(const AttendanceFilter& filter) {
    return attendance_.find(filter);
}

int AttendanceService::markClass(
    long long classId, const std::string& date,
    const std::vector<std::pair<long long, AttendanceStatus>>& entries,
    const std::optional<std::string>& time) {
    if (!classes_.exists(classId)) throw NotFoundError("Classe", classId);

    Validator validator;
    validator.date("date", date).time("time", time);
    if (entries.empty()) validator.add("entries", "Aucun releve fourni");
    validator.throwIfInvalid();

    // Les eleves doivent appartenir a la classe visee.
    const auto students = students_.findByClass(classId, /*activeOnly=*/false);
    int created = 0;
    for (const auto& [studentId, status] : entries) {
        const bool belongs =
            std::any_of(students.begin(), students.end(),
                        [id = studentId](const Student& s) { return s.id == id; });
        if (!belongs) {
            throw ValidationError("L'eleve " + std::to_string(studentId) +
                                  " n'appartient pas a cette classe");
        }
        Attendance value;
        value.studentId = studentId;
        value.date = date;
        value.time = time;
        value.status = status;
        attendance_.create(value);
        ++created;
    }
    LOG_INFO("attendance.service", "Appel enregistre: classe=" + std::to_string(classId) +
                                       " releves=" + std::to_string(created));
    return created;
}

AttendanceSummary AttendanceService::summary(long long studentId) {
    if (!students_.findById(studentId).has_value()) throw NotFoundError("Eleve", studentId);
    return attendance_.summaryForStudent(studentId);
}

nlohmann::json AttendanceService::classStatistics(long long classId) {
    if (!classes_.exists(classId)) throw NotFoundError("Classe", classId);

    long long total = 0;
    long long present = 0;
    long long absent = 0;
    long long excused = 0;
    long long late = 0;
    for (const auto& summary : attendance_.summaryForClass(classId)) {
        total += summary.total;
        present += summary.present;
        absent += summary.absent;
        excused += summary.excused;
        late += summary.late;
    }
    const double rate =
        total > 0 ? static_cast<double>(present + late) * 100.0 / static_cast<double>(total) : 0.0;

    return {{"total", total},       {"present", present}, {"absent", absent},
            {"excused", excused},   {"late", late},       {"attendance_rate", rate}};
}

std::vector<std::pair<std::string, long long>> AttendanceService::monthlyAbsences(
    std::optional<long long> classId) {
    return attendance_.monthlyAbsences(classId);
}

}  // namespace app
