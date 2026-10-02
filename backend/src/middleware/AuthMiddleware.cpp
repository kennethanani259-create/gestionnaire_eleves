#include "middleware/AuthMiddleware.hpp"

#include "core/Error.hpp"
#include "core/Logger.hpp"
#include "core/Tenant.hpp"

namespace app::middleware {
namespace {

/// Un thread httplib ne traite qu'une requete a la fois : thread_local convient.
thread_local std::optional<User> t_currentUser;

UserRole requiredRole(api::Access access) {
    switch (access) {
        case api::Access::Admin: return UserRole::Admin;
        case api::Access::Teacher: return UserRole::Teacher;
        case api::Access::Viewer: return UserRole::Viewer;
        // Parent est le plus bas niveau : exiger ce role revient a n'exiger
        // qu'une authentification valide.
        case api::Access::Authenticated:
        case api::Access::Public: return UserRole::Parent;
    }
    return UserRole::Viewer;
}

}  // namespace

void CurrentUser::set(const User& user) {
    t_currentUser = user;
    // La portee d'etablissement decoule du compte authentifie : aucune route
    // n'a a la poser elle-meme, donc aucune ne peut l'oublier.
    tenant::setCurrentSchool(user.schoolId);
}
void CurrentUser::clear() {
    t_currentUser.reset();
    tenant::setCurrentSchool(std::nullopt);
}
std::optional<User> CurrentUser::peek() { return t_currentUser; }

const User& CurrentUser::require() {
    if (!t_currentUser.has_value()) throw AuthError("Authentification requise");
    return *t_currentUser;
}

namespace {

/// Retire les espaces de part et d'autre d'une chaine.
std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t");
    return value.substr(first, last - first + 1);
}

/// Lit un cookie dans l'en-tete "Cookie: a=1; b=2".
std::optional<std::string> cookieValue(const httplib::Request& req, const std::string& name) {
    if (!req.has_header("Cookie")) return std::nullopt;
    const std::string header = req.get_header_value("Cookie");
    std::size_t position = 0;
    while (position < header.size()) {
        const auto separator = header.find(';', position);
        const std::string pair =
            trim(header.substr(position, separator == std::string::npos ? std::string::npos
                                                                        : separator - position));
        const auto equals = pair.find('=');
        if (equals != std::string::npos && trim(pair.substr(0, equals)) == name) {
            const std::string value = trim(pair.substr(equals + 1));
            if (!value.empty()) return value;
        }
        if (separator == std::string::npos) break;
        position = separator + 1;
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> AuthMiddleware::extractToken(const httplib::Request& req) {
    // 1. En-tete standard "Authorization: Bearer <jeton>".
    if (req.has_header("Authorization")) {
        const std::string header = req.get_header_value("Authorization");
        constexpr const char* kPrefix = "Bearer ";
        if (header.rfind(kPrefix, 0) == 0) {
            const std::string token = header.substr(std::string(kPrefix).size());
            if (!token.empty()) return token;
        }
    }

    // 2. En-tete alternatif. Certains proxys (passerelles d'entreprise,
    //    environnements de previsualisation) consomment ou suppriment
    //    l'en-tete Authorization : on accepte alors X-Auth-Token.
    if (req.has_header("X-Auth-Token")) {
        const std::string token = trim(req.get_header_value("X-Auth-Token"));
        if (!token.empty()) return token;
    }

    // 3. Cookie, utile quand aucun en-tete personnalise ne passe.
    if (const auto token = cookieValue(req, "ge_token")) return token;

    // 4. Parametre de requete, indispensable pour les telechargements ouverts
    //    directement par le navigateur (PDF, CSV) sans passer par fetch().
    if (req.has_param("access_token")) {
        const std::string token = trim(req.get_param_value("access_token"));
        if (!token.empty()) return token;
    }

    return std::nullopt;
}

void AuthMiddleware::operator()(const httplib::Request& req, api::Access access) const {
    CurrentUser::clear();

    const auto token = extractToken(req);

    // Une route publique reste accessible, mais si un jeton valide est fourni
    // on renseigne tout de meme le contexte (utile pour /api/auth/me).
    if (access == api::Access::Public) {
        if (token.has_value()) {
            try {
                CurrentUser::set(auth_.authenticate(*token));
            } catch (const AppException&) {
                // Jeton invalide sur une route publique : simplement ignore.
            }
        }
        return;
    }

    if (!token.has_value()) {
        // Diagnostic : on journalise le NOM des en-tetes recus (jamais leur
        // valeur) afin de distinguer un client qui n'envoie rien d'un proxy
        // qui supprime l'en-tete d'authentification en chemin.
        std::string names;
        for (const auto& header : req.headers) {
            if (!names.empty()) names += ", ";
            names += header.first;
        }
        LOG_DEBUG("auth", "Requete sans jeton sur " + req.path + " | en-tetes recus: " + names);

        throw AuthError(
            "Authentification requise: fournissez le jeton via l'en-tete "
            "'Authorization: Bearer <jeton>' ou 'X-Auth-Token'");
    }

    const User user = auth_.authenticate(*token);  // leve AuthError/ForbiddenError
    AuthService::requireRole(user.role, requiredRole(access));
    CurrentUser::set(user);
}

}  // namespace app::middleware
