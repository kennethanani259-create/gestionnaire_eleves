#pragma once
/**
 * @file Error.hpp
 * @brief Hierarchie d'exceptions applicatives.
 *
 * Chaque exception porte un code HTTP et un code machine ; le middleware
 * ErrorHandler (etape 7) les traduit en reponses JSON normalisees :
 *   { "error": { "code": "VALIDATION_ERROR", "message": "...", "details": {...} } }
 */
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace app {

/// Exception de base de l'application.
class AppException : public std::runtime_error {
public:
    AppException(int httpStatus, std::string code, const std::string& message,
                 nlohmann::json details = nlohmann::json::object())
        : std::runtime_error(message),
          httpStatus_(httpStatus),
          code_(std::move(code)),
          details_(std::move(details)) {}

    int httpStatus() const noexcept { return httpStatus_; }
    const std::string& code() const noexcept { return code_; }
    const nlohmann::json& details() const noexcept { return details_; }

private:
    int httpStatus_;
    std::string code_;
    nlohmann::json details_;
};

/// 400 — donnees invalides.
class ValidationError : public AppException {
public:
    explicit ValidationError(const std::string& message,
                             nlohmann::json details = nlohmann::json::object())
        : AppException(400, "VALIDATION_ERROR", message, std::move(details)) {}
};

/// 401 — authentification requise ou invalide.
class AuthError : public AppException {
public:
    explicit AuthError(const std::string& message = "Authentification requise")
        : AppException(401, "UNAUTHORIZED", message) {}
};

/// 403 — authentifie mais droits insuffisants.
class ForbiddenError : public AppException {
public:
    explicit ForbiddenError(const std::string& message = "Acces interdit")
        : AppException(403, "FORBIDDEN", message) {}
};

/// 404 — ressource inexistante.
class NotFoundError : public AppException {
public:
    explicit NotFoundError(const std::string& resource, long long id)
        : AppException(404, "NOT_FOUND",
                       resource + " introuvable (id=" + std::to_string(id) + ")") {}
    explicit NotFoundError(const std::string& message)
        : AppException(404, "NOT_FOUND", message) {}
};

/// 409 — conflit (doublon, contrainte d'unicite).
class ConflictError : public AppException {
public:
    explicit ConflictError(const std::string& message)
        : AppException(409, "CONFLICT", message) {}
};

/// 500 — erreur de la couche base de donnees.
class DatabaseError : public AppException {
public:
    explicit DatabaseError(const std::string& message)
        : AppException(500, "DATABASE_ERROR", message) {}
};

}  // namespace app
