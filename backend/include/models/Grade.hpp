#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "models/Enums.hpp"

namespace app {

/// Note obtenue par un eleve dans une matiere.
struct Grade {
    long long id = 0;
    long long studentId = 0;
    long long subjectId = 0;
    EvalType evalType = EvalType::Homework;
    double score = 0.0;
    double maxScore = 20.0;
    std::string evalDate;     ///< YYYY-MM-DD
    int term = 1;             ///< trimestre 1..3
    std::optional<std::string> comment;
    std::string createdAt;
    std::string updatedAt;

    // Champs derives (jointures).
    std::optional<std::string> subjectName;
    std::optional<std::string> studentName;
    std::optional<double> coefficient;

    /// Note ramenee sur 20 (base de tous les calculs de moyenne).
    double normalized20() const { return maxScore > 0 ? score * 20.0 / maxScore : 0.0; }

    nlohmann::json toJson() const;
};

}  // namespace app
