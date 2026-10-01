#include "services/AuthService.hpp"

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "utils/Crypto.hpp"
#include "utils/Validator.hpp"

namespace app {

nlohmann::json AuthResult::toJson() const {
    return {{"token", token}, {"token_type", "Bearer"}, {"expires_in", expiresIn},
            {"user", user.toJson()}};
}

void AuthService::requireRole(UserRole actual, UserRole required) {
    if (privilegeLevel(actual) < privilegeLevel(required)) {
        throw ForbiddenError("Votre role (" + label(actual) +
                             ") ne permet pas cette action. Role requis: " + label(required));
    }
}

AuthResult AuthService::login(const std::string& identifier, const std::string& password) {
    if (identifier.empty() || password.empty()) {
        throw ValidationError("Identifiant et mot de passe obligatoires");
    }

    auto user = users_.findByUsername(identifier);
    if (!user.has_value()) user = users_.findByEmail(identifier);

    // Message volontairement identique dans tous les cas d'echec : ne jamais
    // reveler si un compte existe (enumeration d'utilisateurs).
    const AuthError genericError("Identifiant ou mot de passe incorrect");

    if (!user.has_value()) {
        // Calcul factice pour egaliser le temps de reponse avec un compte existant.
        crypto::verifyPassword(password, crypto::hashPassword("mot-de-passe-factice", 1000));
        LOG_WARN("auth", "Echec de connexion: identifiant inconnu (" + identifier + ")");
        throw genericError;
    }
    if (!crypto::verifyPassword(password, user->passwordHash)) {
        LOG_WARN("auth", "Echec de connexion: mot de passe incorrect pour " + user->username);
        throw genericError;
    }
    if (!user->isActive) {
        LOG_WARN("auth", "Tentative de connexion sur un compte desactive: " + user->username);
        throw ForbiddenError("Ce compte est desactive");
    }

    users_.touchLastLogin(user->id);

    jwt::Claims claims;
    claims.userId = user->id;
    claims.username = user->username;
    claims.role = toString(user->role);

    AuthResult result;
    result.token = jwt::encode(claims, secret_, ttlMinutes_);
    result.expiresIn = static_cast<long long>(ttlMinutes_) * 60;
    result.user = *user;
    LOG_INFO("auth", "Connexion reussie: " + user->username + " (" + toString(user->role) + ")");
    return result;
}

User AuthService::authenticate(const std::string& token) {
    const auto claims = jwt::decode(token, secret_);  // leve AuthError si invalide
    auto user = users_.findById(claims.userId);
    if (!user.has_value()) throw AuthError("Compte associe au jeton introuvable");
    if (!user->isActive) throw ForbiddenError("Ce compte est desactive");
    return *user;
}

void AuthService::validateCredentials(const std::string& username, const std::string& email,
                                      const std::string& password, const std::string& fullName,
                                      std::optional<long long> existingId) {
    Validator validator;
    validator.required("username", username)
        .minLength("username", username, 3)
        .maxLength("username", username, 50)
        .required("full_name", fullName)
        .maxLength("full_name", fullName, 120)
        .required("email", email)
        .email("email", email);

    if (!password.empty()) {
        validator.minLength("password", password, 8).maxLength("password", password, 200);
    }

    const auto byUsername = users_.findByUsername(username);
    if (byUsername.has_value() && (!existingId.has_value() || byUsername->id != *existingId)) {
        validator.add("username", "Ce nom d'utilisateur est deja pris");
    }
    const auto byEmail = users_.findByEmail(email);
    if (byEmail.has_value() && (!existingId.has_value() || byEmail->id != *existingId)) {
        validator.add("email", "Cette adresse e-mail est deja utilisee");
    }
    validator.throwIfInvalid();
}

User AuthService::createUser(const std::string& username, const std::string& email,
                             const std::string& password, const std::string& fullName,
                             UserRole role) {
    validateCredentials(username, email, password, fullName, std::nullopt);
    if (password.empty()) {
        throw ValidationError("Mot de passe obligatoire",
                              {{"fields", {{"password", "Ce champ est obligatoire"}}}});
    }

    User user;
    user.username = username;
    user.email = email;
    user.passwordHash = crypto::hashPassword(password);
    user.fullName = fullName;
    user.role = role;
    user.isActive = true;

    const long long id = users_.create(user);
    LOG_INFO("auth", "Compte cree: " + username + " (" + toString(role) + ")");
    return getUser(id);
}

User AuthService::updateUser(long long id, const std::string& username, const std::string& email,
                             const std::string& fullName, UserRole role, bool isActive) {
    auto existing = users_.findById(id);
    if (!existing.has_value()) throw NotFoundError("Utilisateur", id);

    validateCredentials(username, email, "", fullName, id);

    existing->username = username;
    existing->email = email;
    existing->fullName = fullName;
    existing->role = role;
    existing->isActive = isActive;
    users_.update(*existing);
    return getUser(id);
}

void AuthService::changePassword(long long userId, const std::string& currentPassword,
                                 const std::string& newPassword) {
    auto user = users_.findById(userId);
    if (!user.has_value()) throw NotFoundError("Utilisateur", userId);
    if (!crypto::verifyPassword(currentPassword, user->passwordHash)) {
        throw AuthError("Mot de passe actuel incorrect");
    }
    if (newPassword.size() < 8) {
        throw ValidationError(
            "Nouveau mot de passe trop court",
            {{"fields", {{"new_password", "Doit contenir au moins 8 caracteres"}}}});
    }
    users_.updatePassword(userId, crypto::hashPassword(newPassword));
    LOG_INFO("auth", "Mot de passe modifie pour " + user->username);
}

void AuthService::resetPassword(long long userId, const std::string& newPassword) {
    auto user = users_.findById(userId);
    if (!user.has_value()) throw NotFoundError("Utilisateur", userId);
    if (newPassword.size() < 8) {
        throw ValidationError(
            "Mot de passe trop court",
            {{"fields", {{"password", "Doit contenir au moins 8 caracteres"}}}});
    }
    users_.updatePassword(userId, crypto::hashPassword(newPassword));
    LOG_WARN("auth", "Mot de passe reinitialise par un administrateur pour " + user->username);
}

void AuthService::removeUser(long long id, long long requesterId) {
    if (id == requesterId) {
        throw ConflictError("Vous ne pouvez pas supprimer votre propre compte");
    }
    auto user = users_.findById(id);
    if (!user.has_value()) throw NotFoundError("Utilisateur", id);

    // Toujours conserver au moins un administrateur actif.
    if (user->role == UserRole::Admin) {
        int activeAdmins = 0;
        for (const auto& other : users_.findAll()) {
            if (other.role == UserRole::Admin && other.isActive) ++activeAdmins;
        }
        if (activeAdmins <= 1) {
            throw ConflictError("Impossible de supprimer le dernier administrateur actif");
        }
    }
    users_.remove(id);
    LOG_WARN("auth", "Compte supprime: " + user->username);
}

std::vector<User> AuthService::listUsers() { return users_.findAll(); }

User AuthService::getUser(long long id) {
    auto user = users_.findById(id);
    if (!user.has_value()) throw NotFoundError("Utilisateur", id);
    return *user;
}

std::optional<std::string> AuthService::ensureInitialAdmin() {
    if (users_.count() > 0) return std::nullopt;

    // Mot de passe initial aleatoire, affiche une seule fois dans les logs.
    const std::string password = crypto::toHex(crypto::randomBytes(9));
    User admin;
    admin.username = "admin";
    admin.email = "admin@ecole.local";
    admin.passwordHash = crypto::hashPassword(password);
    admin.fullName = "Administrateur";
    admin.role = UserRole::Admin;
    admin.isActive = true;
    users_.create(admin);

    LOG_WARN("auth", "=====================================================");
    LOG_WARN("auth", " Compte administrateur initial cree");
    LOG_WARN("auth", "   identifiant : admin");
    LOG_WARN("auth", "   mot de passe: " + password);
    LOG_WARN("auth", " Changez-le des la premiere connexion.");
    LOG_WARN("auth", "=====================================================");
    return password;
}

}  // namespace app
