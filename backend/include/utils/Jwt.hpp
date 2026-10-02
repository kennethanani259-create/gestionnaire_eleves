#pragma once
/**
 * @file Jwt.hpp
 * @brief Jetons JWT signes en HS256 (HMAC-SHA256).
 *
 * Seul l'algorithme HS256 est accepte : un jeton declarant "alg":"none" ou
 * un autre algorithme est systematiquement rejete (attaque classique
 * de confusion d'algorithme).
 */
#include <string>

#include <nlohmann/json.hpp>

namespace app::jwt {

/// Contenu verifie d'un jeton.
struct Claims {
    long long userId = 0;
    std::string username;
    std::string role;
    long long issuedAt = 0;
    long long expiresAt = 0;
};

/// Genere un jeton signe valable ttlMinutes minutes.
std::string encode(const Claims& claims, const std::string& secret, int ttlMinutes);

/// Verifie signature, format et expiration. Leve AuthError (401) sinon.
Claims decode(const std::string& token, const std::string& secret);

}  // namespace app::jwt
