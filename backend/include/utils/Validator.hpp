#pragma once
/**
 * @file Validator.hpp
 * @brief Collecteur de violations de validation.
 *
 * Toutes les erreurs d'une requete sont accumulees puis renvoyees d'un bloc,
 * afin que l'utilisateur corrige l'ensemble du formulaire en une fois :
 *   { "error": { "code": "VALIDATION_ERROR", "details": { "fields": { ... } } } }
 */
#include <map>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace app {

class Validator {
public:
    /// Enregistre une violation sur un champ (la premiere l'emporte).
    Validator& add(const std::string& field, const std::string& message);

    bool valid() const { return errors_.empty(); }
    const std::map<std::string, std::string>& errors() const { return errors_; }
    nlohmann::json toJson() const;
    /// Leve ValidationError si au moins une violation a ete enregistree.
    void throwIfInvalid(const std::string& message = "Donnees invalides") const;

    // --- Regles reutilisables -----------------------------------------------
    Validator& required(const std::string& field, const std::string& value);
    Validator& maxLength(const std::string& field, const std::string& value, size_t max);
    Validator& minLength(const std::string& field, const std::string& value, size_t min);
    Validator& email(const std::string& field, const std::string& value);
    Validator& optionalEmail(const std::string& field, const std::optional<std::string>& value);
    Validator& phone(const std::string& field, const std::optional<std::string>& value);
    Validator& date(const std::string& field, const std::string& value);
    Validator& time(const std::string& field, const std::optional<std::string>& value);
    Validator& pastDate(const std::string& field, const std::string& value);
    Validator& range(const std::string& field, double value, double min, double max);
    Validator& positive(const std::string& field, double value);
    Validator& intRange(const std::string& field, long long value, long long min, long long max);

    /// Validation d'un format d'e-mail (utilisable hors Validator).
    static bool isEmail(const std::string& value);
    /// Validation d'un numero de telephone (chiffres, espaces, +, -, points, parentheses).
    static bool isPhone(const std::string& value);

private:
    std::map<std::string, std::string> errors_;
};

}  // namespace app
