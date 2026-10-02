#pragma once
/**
 * @file AuthMiddleware.hpp
 * @brief Garde d'authentification et d'autorisation applique avant chaque handler.
 *
 * L'utilisateur authentifie est expose aux controleurs via un contexte
 * propre au thread courant (httplib traite chaque requete dans un thread
 * de son pool), ce qui evite de propager l'etat dans toutes les signatures.
 */
#include <optional>
#include <string>

#include <httplib/httplib.h>

#include "api/Router.hpp"
#include "models/User.hpp"
#include "services/AuthService.hpp"

namespace app::middleware {

/// Utilisateur courant de la requete en cours de traitement.
class CurrentUser {
public:
    static void set(const User& user);
    static void clear();
    /// Utilisateur authentifie ; leve AuthError s'il n'y en a pas.
    static const User& require();
    static std::optional<User> peek();
};

/// Remet le contexte a zero a la fin de la requete, meme en cas d'exception.
class CurrentUserScope {
public:
    explicit CurrentUserScope(const User& user) { CurrentUser::set(user); }
    ~CurrentUserScope() { CurrentUser::clear(); }
    CurrentUserScope(const CurrentUserScope&) = delete;
    CurrentUserScope& operator=(const CurrentUserScope&) = delete;
};

class AuthMiddleware {
public:
    explicit AuthMiddleware(AuthService& auth) : auth_(auth) {}

    /// Garde a brancher sur le Router.
    void operator()(const httplib::Request& req, api::Access access) const;

    /// Extrait le jeton de l'en-tete Authorization: Bearer <jeton>.
    static std::optional<std::string> extractToken(const httplib::Request& req);

private:
    AuthService& auth_;
};

}  // namespace app::middleware
