#include "services/AuthService.hpp"

#include <cctype>

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "core/Tenant.hpp"
#include "utils/Crypto.hpp"
#include "utils/Validator.hpp"

namespace app {

nlohmann::json AuthResult::toJson() const {
    return {{"token", token}, {"token_type", "Bearer"}, {"expires_in", expiresIn},
            {"user", user.toJson()}};
}

nlohmann::json RegistrationResult::toJson() const {
    nlohmann::json j =
           {{"user", user.toJson()},
            {"pending_approval", pendingApproval},
            {"message", pendingApproval
                            ? std::string("Demande enregistree. Un administrateur doit valider "
                                          "votre compte avant la premiere connexion.")
                            : std::string("Compte cree. Vous pouvez vous connecter.")}};
    if (school.has_value()) j["school"] = school->toPublicJson();
    return j;
}

void AuthService::requireRole(UserRole actual, UserRole required) {
    if (privilegeLevel(actual) < privilegeLevel(required)) {
        throw ForbiddenError("Votre role (" + label(actual) +
                             ") ne permet pas cette action. Role requis: " + label(required));
    }
}

std::optional<School> AuthService::schoolOf(const User& user) {
    if (!user.schoolId.has_value()) return std::nullopt;
    // Lecture hors portee : a cet instant la portee n'est pas encore posee.
    tenant::SystemScope systemScope;
    return schools_.findById(*user.schoolId);
}

std::optional<School> AuthService::schoolByCode(const std::string& code) {
    tenant::SystemScope systemScope;
    return schools_.findByCode(code);
}

School AuthService::regenerateSchoolCode(const User& admin) {
    if (!admin.schoolId.has_value()) throw NotFoundError("Aucun etablissement rattache");
    tenant::SystemScope systemScope;
    auto school = schools_.findById(*admin.schoolId);
    if (!school.has_value()) throw NotFoundError("Etablissement", *admin.schoolId);
    school->code = schools_.generateCode();
    schools_.update(*school);
    LOG_WARN("auth", "Matricule regenere pour l'etablissement " + school->name +
                         " par " + admin.username);
    return *school;
}

AuthResult AuthService::login(const std::string& identifier, const std::string& password,
                              std::optional<UserRole> expectedRole) {
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
        throw ForbiddenError(
            "Ce compte n'est pas encore actif. Un administrateur doit le valider.");
    }

    // Le role choisi a l'ecran doit correspondre au compte. Ce controle ne
    // peut qu'interdire une connexion, jamais elargir des droits : le role
    // effectif reste celui enregistre en base.
    if (expectedRole.has_value() && *expectedRole != user->role) {
        LOG_WARN("auth", "Role attendu " + toString(*expectedRole) + " mais le compte " +
                             user->username + " est " + toString(user->role));
        throw ForbiddenError("Ce compte n'est pas un compte " + label(*expectedRole) +
                             ". Choisissez " + label(user->role) + " pour vous connecter.");
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

void AuthService::validatePasswordStrength(Validator& validator, const std::string& password) {
    // Un mot de passe choisi depuis une page publique merite une exigence plus
    // elevee que celle d'un compte cree par un administrateur.
    if (password.size() < 10) {
        validator.add("password", "Au moins 10 caracteres sont necessaires");
        return;
    }
    bool hasLetter = false;
    bool hasDigit = false;
    for (unsigned char c : password) {
        if (std::isalpha(c) != 0) hasLetter = true;
        if (std::isdigit(c) != 0) hasDigit = true;
    }
    if (!hasLetter || !hasDigit) {
        validator.add("password", "Melangez au moins des lettres et des chiffres");
    }
}

RegistrationResult AuthService::registerSchool(const std::string& schoolName,
                                               const std::optional<std::string>& city,
                                               const std::string& username,
                                               const std::string& email,
                                               const std::string& password,
                                               const std::string& fullName) {
    if (selfRegistration_ == SelfRegistration::Off) {
        throw ForbiddenError("La creation d'etablissement en ligne est desactivee sur ce serveur.");
    }

    // La creation d'ecole se fait hors portee : rien n'existe encore.
    tenant::SystemScope systemScope;

    Validator validator;
    validator.required("school_name", schoolName).maxLength("school_name", schoolName, 150);
    validator.throwIfInvalid();

    validateCredentials(username, email, password, fullName, std::nullopt);
    Validator pwd;
    pwd.required("password", password);
    if (!password.empty()) validatePasswordStrength(pwd, password);
    pwd.throwIfInvalid();

    School school;
    school.code = schools_.generateCode();
    school.name = schoolName;
    school.city = city;
    const long long schoolId = schools_.create(school);
    school.id = schoolId;

    User user;
    user.schoolId = schoolId;
    user.username = username;
    user.email = email;
    user.passwordHash = crypto::hashPassword(password);
    user.fullName = fullName;
    // Fondateur de l'etablissement : administrateur de SON ecole, et d'elle seule.
    user.role = UserRole::Admin;
    user.isActive = true;
    const long long userId = users_.create(user);

    LOG_INFO("auth", "Etablissement cree: " + schoolName + " (matricule " + school.code +
                         ") par " + username);

    RegistrationResult result;
    result.user = *users_.findById(userId);
    result.pendingApproval = false;
    result.school = school;
    return result;
}

RegistrationResult AuthService::joinSchool(const std::string& code, UserRole role,
                                           const std::string& username, const std::string& email,
                                           const std::string& password,
                                           const std::string& fullName) {
    if (selfRegistration_ == SelfRegistration::Off) {
        throw ForbiddenError(
            "Les inscriptions en ligne sont desactivees. "
            "Adressez-vous a l'administration de l'etablissement.");
    }
    // Un matricule ne confere jamais les pleins pouvoirs sur une ecole
    // existante : seul le fondateur, ou un administrateur deja en place,
    // peut accorder ce role.
    if (role == UserRole::Admin) {
        throw ForbiddenError(
            "Le role d'administrateur ne s'obtient pas avec un matricule. "
            "Demandez a l'administration de l'etablissement de vous l'accorder.");
    }

    tenant::SystemScope systemScope;

    const auto school = schools_.findByCode(code);
    if (!school.has_value() || !school->isActive) {
        throw ValidationError("Matricule d'etablissement inconnu",
                              {{"fields", {{"school_code", "Aucun etablissement avec ce matricule"}}}});
    }

    validateCredentials(username, email, password, fullName, std::nullopt);
    Validator pwd;
    pwd.required("password", password);
    if (!password.empty()) validatePasswordStrength(pwd, password);
    pwd.throwIfInvalid();

    User user;
    user.schoolId = school->id;
    user.username = username;
    user.email = email;
    user.passwordHash = crypto::hashPassword(password);
    user.fullName = fullName;
    user.role = role;
    user.isActive = (selfRegistration_ == SelfRegistration::Open);

    const long long id = users_.create(user);
    LOG_INFO("auth", "Inscription dans " + school->name + ": " + username + " (" +
                         toString(role) + (user.isActive ? ", active)" : ", en attente)"));

    RegistrationResult result;
    result.user = *users_.findById(id);
    result.pendingApproval = !user.isActive;
    result.school = school;
    return result;
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
    // Rattachement implicite a l'etablissement de l'administrateur courant.
    user.schoolId = tenant::currentSchool();

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
    // Amorcage : aucun compte n'existe encore, donc aucune portee n'est posee.
    tenant::SystemScope systemScope;
    if (users_.count() > 0) return std::nullopt;

    // Tout compte appartient a un etablissement : si la base est vierge, on
    // en cree un pour accueillir l'administrateur initial. Les autres ecoles
    // se creent ensuite depuis la page d'inscription.
    long long schoolId = 0;
    std::string schoolCode;
    auto existing = schools_.findAll();
    if (existing.empty()) {
        School school;
        school.code = schools_.generateCode();
        school.name = "Mon etablissement";
        schoolId = schools_.create(school);
        schoolCode = school.code;
    } else {
        schoolId = existing.front().id;
        schoolCode = existing.front().code;
    }

    // Mot de passe initial aleatoire, affiche une seule fois dans les logs.
    const std::string password = crypto::toHex(crypto::randomBytes(9));
    User admin;
    admin.username = "admin";
    admin.email = "admin@ecole.local";
    admin.passwordHash = crypto::hashPassword(password);
    admin.fullName = "Administrateur";
    admin.role = UserRole::Admin;
    admin.isActive = true;
    admin.schoolId = schoolId;
    users_.create(admin);

    LOG_WARN("auth", "=====================================================");
    LOG_WARN("auth", " Compte administrateur initial cree");
    LOG_WARN("auth", "   identifiant : admin");
    LOG_WARN("auth", "   mot de passe: " + password);
    LOG_WARN("auth", "   etablissement: Mon etablissement");
    LOG_WARN("auth", "   matricule    : " + schoolCode);
    LOG_WARN("auth", " Changez-le des la premiere connexion.");
    LOG_WARN("auth", "=====================================================");
    return password;
}

}  // namespace app
