#pragma once
/**
 * @file School.hpp
 * @brief Etablissement scolaire (racine du cloisonnement multi-etablissements).
 */
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace app {

struct School {
    long long id = 0;
    /// Matricule communique aux membres pour rejoindre l'etablissement.
    std::string code;
    std::string name;
    std::optional<std::string> city;
    std::optional<std::string> country;
    std::optional<std::string> phone;
    std::optional<std::string> email;
    std::optional<std::string> address;
    bool isActive = true;
    std::string createdAt;
    std::string updatedAt;

    nlohmann::json toJson() const;
    /// Vue publique : nom et ville seulement, sans le matricule ni les
    /// coordonnees. Utilisee par la page d'inscription pour confirmer
    /// l'ecole saisie sans rien divulguer d'autre.
    nlohmann::json toPublicJson() const;
};

}  // namespace app
