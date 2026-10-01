#include "middleware/AuthMiddleware.hpp"

#include "core/Error.hpp"

namespace app::middleware {
namespace {

/// Un thread httplib ne traite qu'une requete a la fois : thread_local convient.
thread_local std::optional<User> t_currentUser;

UserRole requiredRole(api::Access access) {
    switch (access) {
        case api::Access::Admin: return UserRole::Admin;
        case api::Access::Teacher: return UserRole::Teacher;
        case api::Access::Viewer:
        case api::Access::Public: return UserRole::Viewer;
    }
    return UserRole::Viewer;
}

}  // namespace

void CurrentUser::set(const User& user) { t_currentUser = user; }
void CurrentUser::clear() { t_currentUser.reset(); }
std::optional<User> CurrentUser::peek() { return t_currentUser; }

const User& CurrentUser::require() {
    if (!t_currentUser.has_value()) throw AuthError("Authentification requise");
    return *t_currentUser;
}

std::optional<std::string> AuthMiddleware::extractToken(const httplib::Request& req) {
    if (!req.has_header("Authorization")) return std::nullopt;
    const std::string header = req.get_header_value("Authorization");
    constexpr const char* kPrefix = "Bearer ";
    if (header.rfind(kPrefix, 0) != 0) return std::nullopt;
    const std::string token = header.substr(std::string(kPrefix).size());
    if (token.empty()) return std::nullopt;
    return token;
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
        throw AuthError("Authentification requise: en-tete 'Authorization: Bearer <jeton>' absent");
    }

    const User user = auth_.authenticate(*token);  // leve AuthError/ForbiddenError
    AuthService::requireRole(user.role, requiredRole(access));
    CurrentUser::set(user);
}

}  // namespace app::middleware
