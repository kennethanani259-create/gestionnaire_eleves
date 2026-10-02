#pragma once
/**
 * @file ErrorHandler.hpp
 * @brief Gestion globale des erreurs : aucune exception ne doit atteindre le serveur.
 *
 * Toute exception levee par un controleur est convertie en reponse JSON
 * normalisee et journalisee avec son contexte :
 *   { "error": { "code": "...", "message": "...", "details": {...} } }
 */
#include <functional>
#include <string>

#include <httplib/httplib.h>

namespace app::middleware {

using Handler = std::function<void(const httplib::Request&, httplib::Response&)>;

/// Encapsule un handler pour capturer toutes les exceptions.
Handler withErrorHandling(std::string route, Handler handler);

/// Construit le corps JSON d'une erreur.
std::string errorBody(const std::string& code, const std::string& message);

}  // namespace app::middleware
