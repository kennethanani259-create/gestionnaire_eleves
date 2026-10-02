#pragma once
/**
 * @file Http.hpp
 * @brief Aides de lecture des requetes et d'ecriture des reponses JSON.
 *
 * Centralise le parsing afin qu'aucun controleur ne manipule directement
 * des chaines brutes non validees.
 */
#include <optional>
#include <string>

#include <httplib/httplib.h>
#include <nlohmann/json.hpp>

#include "models/Enums.hpp"

namespace app::http {

/// Ecrit une reponse JSON avec le code HTTP demande.
void sendJson(httplib::Response& res, int status, const nlohmann::json& body);
/// Reponse 204 sans contenu.
void sendNoContent(httplib::Response& res);

/// Parse le corps JSON. Leve ValidationError (400) si le corps est absent ou mal forme.
nlohmann::json parseBody(const httplib::Request& req);

/// Parametre de chemin numerique (ex: /api/students/42). Leve ValidationError si non numerique.
long long pathId(const httplib::Request& req, size_t index = 1);

std::optional<std::string> queryString(const httplib::Request& req, const std::string& key);
std::optional<long long> queryInt(const httplib::Request& req, const std::string& key);
std::optional<bool> queryBool(const httplib::Request& req, const std::string& key);
int queryIntOr(const httplib::Request& req, const std::string& key, int fallback);

// --- Lecture typee d'un objet JSON (leve ValidationError si le type est incorrect) ---
std::string requireString(const nlohmann::json& body, const std::string& key);
std::string optionalString(const nlohmann::json& body, const std::string& key,
                           const std::string& fallback = "");
std::optional<std::string> nullableString(const nlohmann::json& body, const std::string& key);
double requireNumber(const nlohmann::json& body, const std::string& key);
double optionalNumber(const nlohmann::json& body, const std::string& key, double fallback);
long long requireId(const nlohmann::json& body, const std::string& key);
std::optional<long long> nullableId(const nlohmann::json& body, const std::string& key);
int optionalInt(const nlohmann::json& body, const std::string& key, int fallback);
bool optionalBool(const nlohmann::json& body, const std::string& key, bool fallback);

/// Chaine facultative : nullopt si absente ou vide (et non une chaine vide).
std::optional<std::string> optionalStringOpt(const nlohmann::json& body, const std::string& key);

}  // namespace app::http
