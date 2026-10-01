#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

#include "models/Enums.hpp"

namespace app {

/// Compte utilisateur. passwordHash n'est JAMAIS serialise par toJson().
struct User {
    long long id = 0;
    std::string username;
    std::string email;
    std::string passwordHash;
    std::string fullName;
    UserRole role = UserRole::Viewer;
    bool isActive = true;
    std::optional<std::string> lastLoginAt;
    std::string createdAt;
    std::string updatedAt;

    nlohmann::json toJson() const;
};

}  // namespace app
