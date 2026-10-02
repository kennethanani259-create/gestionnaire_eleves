#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "models/Enums.hpp"

namespace app {

/// Eleve.
struct Student {
    long long id = 0;
    std::string matricule;                 ///< identifiant metier unique
    std::string firstName;
    std::string lastName;
    std::string birthDate;                 ///< YYYY-MM-DD
    Gender gender = Gender::Male;
    std::optional<std::string> address;
    std::optional<std::string> phone;
    std::optional<std::string> email;
    std::optional<std::string> guardianName;
    std::optional<std::string> guardianPhone;
    std::string enrollmentDate;            ///< YYYY-MM-DD
    StudentStatus status = StudentStatus::Active;
    std::optional<std::string> photoPath;
    std::optional<long long> classId;
    std::string createdAt;
    std::string updatedAt;

    // Champs derives (jointures).
    std::optional<std::string> className;
    std::optional<std::string> classLevel;

    std::string fullName() const { return firstName + " " + lastName; }
    nlohmann::json toJson() const;
};

}  // namespace app
