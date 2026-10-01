#include "utils/Validator.hpp"

#include <algorithm>
#include <cctype>
#include <regex>

#include "core/Error.hpp"
#include "utils/DateTime.hpp"

namespace app {
namespace {

std::string trim(const std::string& value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return {};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

}  // namespace

bool Validator::isEmail(const std::string& value) {
    // Pragmatique et volontairement strict sur la structure generale.
    static const std::regex kPattern(
        R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)");
    return value.size() <= 254 && std::regex_match(value, kPattern);
}

bool Validator::isPhone(const std::string& value) {
    static const std::regex kPattern(R"(^\+?[0-9 ().\-]{6,20}$)");
    return std::regex_match(value, kPattern);
}

Validator& Validator::add(const std::string& field, const std::string& message) {
    errors_.emplace(field, message);
    return *this;
}

nlohmann::json Validator::toJson() const {
    nlohmann::json fields = nlohmann::json::object();
    for (const auto& [field, message] : errors_) fields[field] = message;
    return nlohmann::json{{"fields", fields}};
}

void Validator::throwIfInvalid(const std::string& message) const {
    if (!valid()) throw ValidationError(message, toJson());
}

Validator& Validator::required(const std::string& field, const std::string& value) {
    if (trim(value).empty()) add(field, "Ce champ est obligatoire");
    return *this;
}

Validator& Validator::maxLength(const std::string& field, const std::string& value, size_t max) {
    if (value.size() > max) {
        add(field, "Ne doit pas depasser " + std::to_string(max) + " caracteres");
    }
    return *this;
}

Validator& Validator::minLength(const std::string& field, const std::string& value, size_t min) {
    if (value.size() < min) {
        add(field, "Doit contenir au moins " + std::to_string(min) + " caracteres");
    }
    return *this;
}

Validator& Validator::email(const std::string& field, const std::string& value) {
    if (!isEmail(value)) add(field, "Adresse e-mail invalide");
    return *this;
}

Validator& Validator::optionalEmail(const std::string& field,
                                    const std::optional<std::string>& value) {
    if (value.has_value() && !trim(*value).empty() && !isEmail(*value)) {
        add(field, "Adresse e-mail invalide");
    }
    return *this;
}

Validator& Validator::phone(const std::string& field, const std::optional<std::string>& value) {
    if (value.has_value() && !trim(*value).empty() && !isPhone(*value)) {
        add(field, "Numero de telephone invalide");
    }
    return *this;
}

Validator& Validator::date(const std::string& field, const std::string& value) {
    if (!datetime::isValidDate(value)) add(field, "Date invalide (format attendu: AAAA-MM-JJ)");
    return *this;
}

Validator& Validator::time(const std::string& field, const std::optional<std::string>& value) {
    if (value.has_value() && !trim(*value).empty() && !datetime::isValidTime(*value)) {
        add(field, "Heure invalide (format attendu: HH:MM)");
    }
    return *this;
}

Validator& Validator::pastDate(const std::string& field, const std::string& value) {
    if (!datetime::isValidDate(value)) {
        add(field, "Date invalide (format attendu: AAAA-MM-JJ)");
    } else if (value > datetime::today()) {
        add(field, "La date ne peut pas etre dans le futur");
    }
    return *this;
}

Validator& Validator::range(const std::string& field, double value, double min, double max) {
    if (value < min || value > max) {
        add(field, "Doit etre compris entre " + std::to_string(min) + " et " +
                       std::to_string(max));
    }
    return *this;
}

Validator& Validator::positive(const std::string& field, double value) {
    if (value <= 0) add(field, "Doit etre strictement positif");
    return *this;
}

Validator& Validator::intRange(const std::string& field, long long value, long long min,
                               long long max) {
    if (value < min || value > max) {
        add(field, "Doit etre compris entre " + std::to_string(min) + " et " +
                       std::to_string(max));
    }
    return *this;
}

}  // namespace app
