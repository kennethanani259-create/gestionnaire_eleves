#pragma once
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace app {

/// Enseignant, eventuellement rattache a un compte utilisateur.
struct Teacher {
    long long id = 0;
    std::optional<long long> userId;
    std::string firstName;
    std::string lastName;
    std::optional<std::string> email;
    std::optional<std::string> phone;
    std::optional<std::string> speciality;
    std::string createdAt;
    std::string updatedAt;

    std::string fullName() const { return firstName + " " + lastName; }
    nlohmann::json toJson() const;
};

}  // namespace app
