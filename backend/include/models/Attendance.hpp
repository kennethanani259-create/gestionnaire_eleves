#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "models/Enums.hpp"

namespace app {

/// Releve de presence d'un eleve a une date (et eventuellement une heure/matiere).
struct Attendance {
    long long id = 0;
    long long studentId = 0;
    std::optional<long long> subjectId;
    std::string date;                  ///< YYYY-MM-DD
    std::optional<std::string> time;   ///< HH:MM
    AttendanceStatus status = AttendanceStatus::Present;
    std::optional<std::string> justification;
    std::optional<std::string> comment;
    std::string createdAt;
    std::string updatedAt;

    // Champs derives (jointures).
    std::optional<std::string> subjectName;
    std::optional<std::string> studentName;

    nlohmann::json toJson() const;
};

}  // namespace app
