#include "models/Enums.hpp"

#include <algorithm>
#include <cctype>

#include "core/Error.hpp"

namespace app {
namespace {

std::string upper(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) out.push_back(static_cast<char>(::toupper(static_cast<unsigned char>(c))));
    return out;
}

[[noreturn]] void unknown(const std::string& type, const std::string& value,
                          const std::string& allowed) {
    throw ValidationError(type + " invalide: '" + value + "'. Valeurs acceptees: " + allowed);
}

}  // namespace

// --------------------------------------------------------------------- Gender
std::string toString(Gender value) { return value == Gender::Male ? "M" : "F"; }

Gender parseGender(const std::string& value) {
    const std::string v = upper(value);
    if (v == "M" || v == "MALE" || v == "MASCULIN" || v == "GARCON") return Gender::Male;
    if (v == "F" || v == "FEMALE" || v == "FEMININ" || v == "FILLE") return Gender::Female;
    unknown("Sexe", value, "M, F");
}

std::string label(Gender value) { return value == Gender::Male ? "Masculin" : "Feminin"; }

// -------------------------------------------------------------- StudentStatus
std::string toString(StudentStatus value) {
    switch (value) {
        case StudentStatus::Active: return "ACTIVE";
        case StudentStatus::Inactive: return "INACTIVE";
        case StudentStatus::Transferred: return "TRANSFERRED";
        case StudentStatus::Expelled: return "EXPELLED";
    }
    return "ACTIVE";
}

StudentStatus parseStudentStatus(const std::string& value) {
    const std::string v = upper(value);
    if (v == "ACTIVE" || v == "ACTIF") return StudentStatus::Active;
    if (v == "INACTIVE" || v == "INACTIF") return StudentStatus::Inactive;
    if (v == "TRANSFERRED" || v == "TRANSFERE") return StudentStatus::Transferred;
    if (v == "EXPELLED" || v == "EXCLU") return StudentStatus::Expelled;
    unknown("Statut d'eleve", value, "ACTIVE, INACTIVE, TRANSFERRED, EXPELLED");
}

std::string label(StudentStatus value) {
    switch (value) {
        case StudentStatus::Active: return "Actif";
        case StudentStatus::Inactive: return "Inactif";
        case StudentStatus::Transferred: return "Transfere";
        case StudentStatus::Expelled: return "Exclu";
    }
    return "Actif";
}

// ------------------------------------------------------------------ EvalType
std::string toString(EvalType value) {
    switch (value) {
        case EvalType::Homework: return "HOMEWORK";
        case EvalType::Quiz: return "QUIZ";
        case EvalType::Exam: return "EXAM";
        case EvalType::Lab: return "LAB";
        case EvalType::Project: return "PROJECT";
        case EvalType::Continuous: return "CONTINUOUS";
    }
    return "HOMEWORK";
}

EvalType parseEvalType(const std::string& value) {
    const std::string v = upper(value);
    if (v == "HOMEWORK" || v == "DEVOIR") return EvalType::Homework;
    if (v == "QUIZ" || v == "INTERROGATION") return EvalType::Quiz;
    if (v == "EXAM" || v == "EXAMEN") return EvalType::Exam;
    if (v == "LAB" || v == "TP") return EvalType::Lab;
    if (v == "PROJECT" || v == "PROJET") return EvalType::Project;
    if (v == "CONTINUOUS" || v == "CONTROLE_CONTINU") return EvalType::Continuous;
    unknown("Type d'evaluation", value, "HOMEWORK, QUIZ, EXAM, LAB, PROJECT, CONTINUOUS");
}

std::string label(EvalType value) {
    switch (value) {
        case EvalType::Homework: return "Devoir";
        case EvalType::Quiz: return "Interrogation";
        case EvalType::Exam: return "Examen";
        case EvalType::Lab: return "TP";
        case EvalType::Project: return "Projet";
        case EvalType::Continuous: return "Controle continu";
    }
    return "Devoir";
}

// ---------------------------------------------------------- AttendanceStatus
std::string toString(AttendanceStatus value) {
    switch (value) {
        case AttendanceStatus::Present: return "PRESENT";
        case AttendanceStatus::Absent: return "ABSENT";
        case AttendanceStatus::Excused: return "EXCUSED";
        case AttendanceStatus::Late: return "LATE";
    }
    return "PRESENT";
}

AttendanceStatus parseAttendanceStatus(const std::string& value) {
    const std::string v = upper(value);
    if (v == "PRESENT") return AttendanceStatus::Present;
    if (v == "ABSENT") return AttendanceStatus::Absent;
    if (v == "EXCUSED" || v == "ABSENT_JUSTIFIE" || v == "JUSTIFIE") return AttendanceStatus::Excused;
    if (v == "LATE" || v == "RETARD") return AttendanceStatus::Late;
    unknown("Statut de presence", value, "PRESENT, ABSENT, EXCUSED, LATE");
}

std::string label(AttendanceStatus value) {
    switch (value) {
        case AttendanceStatus::Present: return "Present";
        case AttendanceStatus::Absent: return "Absent";
        case AttendanceStatus::Excused: return "Absent justifie";
        case AttendanceStatus::Late: return "Retard";
    }
    return "Present";
}

// ------------------------------------------------------------------ UserRole
std::string toString(UserRole value) {
    switch (value) {
        case UserRole::Admin: return "ADMIN";
        case UserRole::Teacher: return "TEACHER";
        case UserRole::Viewer: return "VIEWER";
    }
    return "VIEWER";
}

UserRole parseUserRole(const std::string& value) {
    const std::string v = upper(value);
    if (v == "ADMIN" || v == "ADMINISTRATEUR") return UserRole::Admin;
    if (v == "TEACHER" || v == "ENSEIGNANT") return UserRole::Teacher;
    if (v == "VIEWER" || v == "CONSULTATION") return UserRole::Viewer;
    unknown("Role", value, "ADMIN, TEACHER, VIEWER");
}

std::string label(UserRole value) {
    switch (value) {
        case UserRole::Admin: return "Administrateur";
        case UserRole::Teacher: return "Enseignant";
        case UserRole::Viewer: return "Consultation";
    }
    return "Consultation";
}

int privilegeLevel(UserRole role) {
    switch (role) {
        case UserRole::Admin: return 2;
        case UserRole::Teacher: return 1;
        case UserRole::Viewer: return 0;
    }
    return 0;
}

}  // namespace app
