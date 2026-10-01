#pragma once
/**
 * @file AuthService.hpp
 * @brief Authentification, gestion des comptes et permissions.
 */
#include <optional>
#include <string>
#include <vector>

#include "core/Config.hpp"
#include "utils/Validator.hpp"
#include "repositories/UserRepository.hpp"
#include "utils/Jwt.hpp"

namespace app {

/// Resultat d'une authentification reussie.
/// Resultat d'une inscription autonome.
struct RegistrationResult {
    User user;
    bool pendingApproval = false;  ///< true : le compte attend la validation d'un administrateur
    nlohmann::json toJson() const;
};

struct AuthResult {
    std::string token;
    long long expiresIn = 0;  ///< secondes
    User user;
    nlohmann::json toJson() const;
};

class AuthService {
public:
    AuthService(IUserRepository& users, std::string jwtSecret, int jwtTtlMinutes,
                SelfRegistration selfRegistration = SelfRegistration::Approval)
        : users_(users),
          secret_(std::move(jwtSecret)),
          ttlMinutes_(jwtTtlMinutes),
          selfRegistration_(selfRegistration) {}

    /// Authentifie par identifiant (nom d'utilisateur ou e-mail) et mot de passe.
    AuthResult login(const std::string& identifier, const std::string& password);
    /// Verifie un jeton et retourne l'utilisateur correspondant (compte actif requis).
    User authenticate(const std::string& token);

    /// Politique d'inscription autonome en vigueur.
    SelfRegistration selfRegistrationMode() const { return selfRegistration_; }

    /// Inscription demandee par un visiteur. Le role est TOUJOURS impose a
    /// Consultation : une page publique ne peut pas accorder de droits d'ecriture.
    /// Selon la politique, le compte est actif ou en attente de validation.
    RegistrationResult selfRegister(const std::string& username, const std::string& email,
                                    const std::string& password, const std::string& fullName);

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

    /// Exigences minimales de robustesse pour un mot de passe choisi par le public.
    static void validatePasswordStrength(Validator& validator, const std::string& password);

    IUserRepository& users_;
    std::string secret_;
    int ttlMinutes_;
    SelfRegistration selfRegistration_;
};

}  // namespace app
