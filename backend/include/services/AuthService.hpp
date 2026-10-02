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
#include "repositories/SchoolRepository.hpp"
#include "repositories/UserRepository.hpp"
#include "utils/Jwt.hpp"

namespace app {

/// Resultat d'une authentification reussie.
/// Resultat d'une inscription autonome.
struct RegistrationResult {
    User user;
    bool pendingApproval = false;  ///< true : le compte attend la validation d'un administrateur
    std::optional<School> school;  ///< etablissement rejoint ou cree
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
    AuthService(IUserRepository& users, ISchoolRepository& schools, std::string jwtSecret,
                int jwtTtlMinutes,
                SelfRegistration selfRegistration = SelfRegistration::Open)
        : users_(users),
          schools_(schools),
          secret_(std::move(jwtSecret)),
          ttlMinutes_(jwtTtlMinutes),
          selfRegistration_(selfRegistration) {}

    /**
     * Authentifie par identifiant (nom d'utilisateur ou e-mail) et mot de passe.
     * @param expectedRole si renseigne, la connexion echoue lorsque le compte
     *        ne porte pas exactement ce role. Le role n'est jamais accorde par
     *        ce parametre : il ne sert qu'a verifier le choix fait a l'ecran.
     */
    AuthResult login(const std::string& identifier, const std::string& password,
                     std::optional<UserRole> expectedRole = std::nullopt);
    /// Verifie un jeton et retourne l'utilisateur correspondant (compte actif requis).
    User authenticate(const std::string& token);

    /// Politique d'inscription autonome en vigueur.
    SelfRegistration selfRegistrationMode() const { return selfRegistration_; }

    /// Inscription demandee par un visiteur. Le role est TOUJOURS impose a
    /// Consultation : une page publique ne peut pas accorder de droits d'ecriture.
    /// Selon la politique, le compte est actif ou en attente de validation.
    /**
     * Cree un etablissement et son premier compte, administrateur de plein
     * droit sur cette ecole uniquement. Toujours actif : sans cela, personne
     * ne pourrait valider le compte.
     */
    RegistrationResult registerSchool(const std::string& schoolName,
                                      const std::optional<std::string>& city,
                                      const std::string& username, const std::string& email,
                                      const std::string& password, const std::string& fullName);

    /**
     * Rejoint un etablissement existant via son matricule.
     * @param role role demande, restreint a Teacher/Parent/Viewer : le role
     *        d'administrateur ne s'obtient jamais avec un matricule.
     */
    RegistrationResult joinSchool(const std::string& code, UserRole role,
                                  const std::string& username, const std::string& email,
                                  const std::string& password, const std::string& fullName);

    /// Etablissement d'un compte, ou nullopt s'il n'est rattache a aucun.
    std::optional<School> schoolOf(const User& user);
    /// Recherche publique par matricule (inscription).
    std::optional<School> schoolByCode(const std::string& code);
    /// Nouveau matricule pour l'etablissement de l'administrateur courant :
    /// invalide l'ancien, utile s'il a circule trop largement.
    School regenerateSchoolCode(const User& admin);

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
    ISchoolRepository& schools_;
    std::string secret_;
    int ttlMinutes_;
    SelfRegistration selfRegistration_;
};

}  // namespace app
