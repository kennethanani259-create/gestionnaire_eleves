#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace app {

/// Classe (ex: '6e A'). Nomme ClassRoom car 'class' est un mot-cle C++.
struct ClassRoom {
    long long id = 0;
    std::string name;
    std::string level;
    long long schoolYearId = 0;
    std::optional<long long> mainTeacherId;
    std::optional<std::string> room;
    std::optional<long long> capacity;
    std::string createdAt;
    std::string updatedAt;

    // Champs derives, remplis par les jointures de lecture (jamais ecrits en base).
    std::optional<std::string> schoolYearLabel;
    std::optional<std::string> mainTeacherName;
    long long studentCount = 0;
    long long subjectCount = 0;

    nlohmann::json toJson() const;
};

}  // namespace app
