#pragma once
/**
 * @file AuthService.hpp
 * @brief Authentification, gestion des comptes et permissions.
 */
#include <optional>
#include <string>
#include <vector>

#include "core/Config.hpp"
#include "repositories/UserRepository.hpp"
#include "utils/Jwt.hpp"

namespace app {

/// Resultat d'une authentification reussie.
struct AuthResult {
    std::string token;
    long long expiresIn = 0;  ///< secondes
    User user;
    nlohmann::json toJson() const;
};

class AuthService {
public:
    AuthService(IUserRepository& users, std::string jwtSecret, int jwtTtlMinutes)
        : users_(users), secret_(std::move(jwtSecret)), ttlMinutes_(jwtTtlMinutes) {}

    /// Authentifie par identifiant (nom d'utilisateur ou e-mail) et mot de passe.
    AuthResult login(const std::string& identifier, const std::string& password);
    /// Verifie un jeton et retourne l'utilisateur correspondant (compte actif requis).
    User authenticate(const std::string& token);

    User createUser(const std::string& username, const std::string& email,
                    const std::string& password, const std::string& fullName, UserRole role);
    User updateUser(long long id, const std::string& username, const std::string& email,
                    const std::string& fullName, UserRole role, bool isActive);
    void changePassword(long long userId, const std::string& currentPassword,
                        const std::string& newPassword);
    /// Reinitialisation par un administrateur (sans connaitre l'ancien mot de passe).
    void resetPassword(long long userId, const std::string& newPassword);
    void removeUser(long long id, long long requesterId);
    std::vector<User> listUsers();
    User getUser(long long id);

    /// Cree le compte administrateur initial si aucun utilisateur n'existe.
    std::optional<std::string> ensureInitialAdmin();

    /// Verifie qu'un role satisfait le niveau requis, sinon leve ForbiddenError.
    static void requireRole(UserRole actual, UserRole required);

private:
    void validateCredentials(const std::string& username, const std::string& email,
                             const std::string& password, const std::string& fullName,
                             std::optional<long long> existingId);

    IUserRepository& users_;
    std::string secret_;
    int ttlMinutes_;
};

}  // namespace app
