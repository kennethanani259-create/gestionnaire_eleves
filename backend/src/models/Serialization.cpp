/**
 * @file Serialization.cpp
 * @brief Serialisation JSON des entites (sortie API).
 *
 * Regle de securite : aucune donnee sensible (hash de mot de passe, sel, secret)
 * n'est jamais ecrite ici.
 */
#include <nlohmann/json.hpp>

#include "models/Attendance.hpp"
#include "models/ClassRoom.hpp"
#include "models/Grade.hpp"
#include "models/SchoolYear.hpp"
#include "models/Student.hpp"
#include "models/Subject.hpp"
#include "models/Teacher.hpp"
#include "models/School.hpp"
#include "models/User.hpp"

namespace app {
namespace {

/// Ecrit la cle ou null si l'optionnel est vide.
template <typename T>
void put(nlohmann::json& j, const char* key, const std::optional<T>& value) {
    if (value.has_value()) {
        j[key] = *value;
    } else {
        j[key] = nullptr;
    }
}

}  // namespace

nlohmann::json SchoolYear::toJson() const {
    return {{"id", id},
            {"label", label},
            {"start_date", startDate},
            {"end_date", endDate},
            {"is_current", isCurrent},
            {"created_at", createdAt}};
}

nlohmann::json Teacher::toJson() const {
    nlohmann::json j{{"id", id},
                     {"first_name", firstName},
                     {"last_name", lastName},
                     {"full_name", fullName()},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "user_id", userId);
    put(j, "email", email);
    put(j, "phone", phone);
    put(j, "speciality", speciality);
    return j;
}

nlohmann::json ClassRoom::toJson() const {
    nlohmann::json j{{"id", id},
                     {"name", name},
                     {"level", level},
                     {"school_year_id", schoolYearId},
                     {"student_count", studentCount},
                     {"subject_count", subjectCount},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "main_teacher_id", mainTeacherId);
    put(j, "room", room);
    put(j, "capacity", capacity);
    put(j, "school_year_label", schoolYearLabel);
    put(j, "main_teacher_name", mainTeacherName);
    return j;
}

nlohmann::json Subject::toJson() const {
    nlohmann::json j{{"id", id},
                     {"name", name},
                     {"code", code},
                     {"coefficient", coefficient},
                     {"class_id", classId},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "teacher_id", teacherId);
    put(j, "class_name", className);
    put(j, "teacher_name", teacherName);
    return j;
}

nlohmann::json Student::toJson() const {
    nlohmann::json j{{"id", id},
                     {"matricule", matricule},
                     {"first_name", firstName},
                     {"last_name", lastName},
                     {"full_name", fullName()},
                     {"birth_date", birthDate},
                     {"gender", app::toString(gender)},
                     {"enrollment_date", enrollmentDate},
                     {"status", app::toString(status)},
                     {"status_label", app::label(status)},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "address", address);
    put(j, "phone", phone);
    put(j, "email", email);
    put(j, "guardian_name", guardianName);
    put(j, "guardian_phone", guardianPhone);
    put(j, "photo_path", photoPath);
    put(j, "class_id", classId);
    put(j, "class_name", className);
    put(j, "class_level", classLevel);
    return j;
}

nlohmann::json Grade::toJson() const {
    nlohmann::json j{{"id", id},
                     {"student_id", studentId},
                     {"subject_id", subjectId},
                     {"eval_type", app::toString(evalType)},
                     {"eval_type_label", app::label(evalType)},
                     {"score", score},
                     {"max_score", maxScore},
                     {"score_20", normalized20()},
                     {"eval_date", evalDate},
                     {"term", term},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "comment", comment);
    put(j, "subject_name", subjectName);
    put(j, "student_name", studentName);
    put(j, "coefficient", coefficient);
    return j;
}

nlohmann::json Attendance::toJson() const {
    nlohmann::json j{{"id", id},
                     {"student_id", studentId},
                     {"date", date},
                     {"status", app::toString(status)},
                     {"status_label", app::label(status)},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "subject_id", subjectId);
    put(j, "time", time);
    put(j, "justification", justification);
    put(j, "comment", comment);
    put(j, "subject_name", subjectName);
    put(j, "student_name", studentName);
    return j;
}

nlohmann::json User::toJson() const {
    // passwordHash volontairement absent.
    nlohmann::json j{{"id", id},
                     {"username", username},
                     {"email", email},
                     {"full_name", fullName},
                     {"role", app::toString(role)},
                     {"role_label", app::label(role)},
                     {"is_active", isActive},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "last_login_at", lastLoginAt);
    put(j, "school_id", schoolId);
    return j;
}

// ----------------------------------------------------------------- School
nlohmann::json School::toJson() const {
    nlohmann::json j{{"id", id},
                     {"code", code},
                     {"name", name},
                     {"is_active", isActive},
                     {"created_at", createdAt},
                     {"updated_at", updatedAt}};
    put(j, "city", city);
    put(j, "country", country);
    put(j, "phone", phone);
    put(j, "email", email);
    put(j, "address", address);
    return j;
}

nlohmann::json School::toPublicJson() const {
    // Volontairement minimal : le matricule n'est pas renvoye (celui qui
    // interroge le connait deja) et aucune coordonnee n'est exposee.
    nlohmann::json j{{"id", id}, {"name", name}};
    put(j, "city", city);
    return j;
}

}  // namespace app
