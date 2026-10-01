#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace app {

/// Matiere enseignee dans une classe, avec son coefficient.
struct Subject {
    long long id = 0;
    std::string name;
    std::string code;
    double coefficient = 1.0;
    long long classId = 0;
    std::optional<long long> teacherId;
    std::string createdAt;
    std::string updatedAt;

    // Champs derives (jointures).
    std::optional<std::string> className;
    std::optional<std::string> teacherName;

    nlohmann::json toJson() const;
};

}  // namespace app
