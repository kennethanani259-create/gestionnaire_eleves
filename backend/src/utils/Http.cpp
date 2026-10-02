#include "utils/Http.hpp"

#include <algorithm>
#include <cctype>

#include "core/Error.hpp"

namespace app::http {
namespace {

bool isTruthy(const std::string& value) {
    std::string v;
    for (char c : value) v.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
    return v == "1" || v == "true" || v == "yes" || v == "oui";
}

}  // namespace

void sendJson(httplib::Response& res, int status, const nlohmann::json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}

void sendNoContent(httplib::Response& res) {
    res.status = 204;
    res.set_content("", "application/json");
}

nlohmann::json parseBody(const httplib::Request& req) {
    if (req.body.empty()) throw ValidationError("Corps de requete JSON absent");
    auto parsed = nlohmann::json::parse(req.body, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded()) throw ValidationError("Corps de requete JSON mal forme");
    if (!parsed.is_object()) throw ValidationError("Un objet JSON est attendu");
    return parsed;
}

long long pathId(const httplib::Request& req, size_t index) {
    if (req.matches.size() <= index) throw ValidationError("Identifiant manquant dans l'URL");
    const std::string raw = req.matches[index];
    try {
        size_t consumed = 0;
        const long long id = std::stoll(raw, &consumed);
        if (consumed != raw.size() || id <= 0) {
            throw ValidationError("Identifiant invalide: " + raw);
        }
        return id;
    } catch (const ValidationError&) {
        throw;
    } catch (const std::exception&) {
        throw ValidationError("Identifiant invalide: " + raw);
    }
}

std::optional<std::string> queryString(const httplib::Request& req, const std::string& key) {
    if (!req.has_param(key.c_str())) return std::nullopt;
    const std::string value = req.get_param_value(key.c_str());
    if (value.empty()) return std::nullopt;
    return value;
}

std::optional<long long> queryInt(const httplib::Request& req, const std::string& key) {
    const auto raw = queryString(req, key);
    if (!raw.has_value()) return std::nullopt;
    try {
        size_t consumed = 0;
        const long long value = std::stoll(*raw, &consumed);
        if (consumed != raw->size()) throw ValidationError("Parametre '" + key + "' invalide");
        return value;
    } catch (const ValidationError&) {
        throw;
    } catch (const std::exception&) {
        throw ValidationError("Parametre '" + key + "' invalide: " + *raw);
    }
}

std::optional<bool> queryBool(const httplib::Request& req, const std::string& key) {
    const auto raw = queryString(req, key);
    if (!raw.has_value()) return std::nullopt;
    return isTruthy(*raw);
}

int queryIntOr(const httplib::Request& req, const std::string& key, int fallback) {
    const auto value = queryInt(req, key);
    return value.has_value() ? static_cast<int>(*value) : fallback;
}

std::string requireString(const nlohmann::json& body, const std::string& key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        throw ValidationError("Champ obligatoire manquant: " + key,
                              {{"fields", {{key, "Ce champ est obligatoire"}}}});
    }
    if (!body.at(key).is_string()) {
        throw ValidationError("Le champ '" + key + "' doit etre une chaine");
    }
    return body.at(key).get<std::string>();
}

std::string optionalString(const nlohmann::json& body, const std::string& key,
                           const std::string& fallback) {
    if (!body.contains(key) || body.at(key).is_null()) return fallback;
    if (!body.at(key).is_string()) {
        throw ValidationError("Le champ '" + key + "' doit etre une chaine");
    }
    return body.at(key).get<std::string>();
}

std::optional<std::string> nullableString(const nlohmann::json& body, const std::string& key) {
    if (!body.contains(key) || body.at(key).is_null()) return std::nullopt;
    if (!body.at(key).is_string()) {
        throw ValidationError("Le champ '" + key + "' doit etre une chaine");
    }
    const std::string value = body.at(key).get<std::string>();
    if (value.empty()) return std::nullopt;
    return value;
}

double requireNumber(const nlohmann::json& body, const std::string& key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        throw ValidationError("Champ obligatoire manquant: " + key,
                              {{"fields", {{key, "Ce champ est obligatoire"}}}});
    }
    if (!body.at(key).is_number()) {
        throw ValidationError("Le champ '" + key + "' doit etre un nombre");
    }
    return body.at(key).get<double>();
}

double optionalNumber(const nlohmann::json& body, const std::string& key, double fallback) {
    if (!body.contains(key) || body.at(key).is_null()) return fallback;
    if (!body.at(key).is_number()) {
        throw ValidationError("Le champ '" + key + "' doit etre un nombre");
    }
    return body.at(key).get<double>();
}

long long requireId(const nlohmann::json& body, const std::string& key) {
    const double value = requireNumber(body, key);
    if (value <= 0) throw ValidationError("Le champ '" + key + "' doit etre un identifiant valide");
    return static_cast<long long>(value);
}

std::optional<long long> nullableId(const nlohmann::json& body, const std::string& key) {
    if (!body.contains(key) || body.at(key).is_null()) return std::nullopt;
    if (!body.at(key).is_number()) {
        throw ValidationError("Le champ '" + key + "' doit etre un identifiant numerique");
    }
    const long long value = body.at(key).get<long long>();
    if (value <= 0) return std::nullopt;
    return value;
}

int optionalInt(const nlohmann::json& body, const std::string& key, int fallback) {
    return static_cast<int>(optionalNumber(body, key, fallback));
}

bool optionalBool(const nlohmann::json& body, const std::string& key, bool fallback) {
    if (!body.contains(key) || body.at(key).is_null()) return fallback;
    if (body.at(key).is_boolean()) return body.at(key).get<bool>();
    if (body.at(key).is_number()) return body.at(key).get<double>() != 0;
    if (body.at(key).is_string()) return isTruthy(body.at(key).get<std::string>());
    return fallback;
}

std::optional<std::string> optionalStringOpt(const nlohmann::json& body,
                                             const std::string& key) {
    const std::string value = optionalString(body, key);
    if (value.empty()) return std::nullopt;
    return value;
}

}  // namespace app::http
