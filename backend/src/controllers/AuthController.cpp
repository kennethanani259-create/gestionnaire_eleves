#include "controllers/AuthController.hpp"

#include "core/Error.hpp"
#include "middleware/AuthMiddleware.hpp"
#include "utils/Http.hpp"

namespace app {
namespace http = app::http;
using middleware::CurrentUser;

void RegistrationThrottle::check(const std::string& client) {
    const auto now = std::chrono::steady_clock::now();
    const std::lock_guard<std::mutex> lock(mutex_);
    auto& hits = hits_[client];
    while (!hits.empty() && now - hits.front() > window_) hits.pop_front();
    if (hits.size() >= maxPerWindow_) {
        throw AppException(429, "TOO_MANY_REQUESTS",
                           "Trop de demandes de compte depuis cet appareil. "
                           "Reessayez dans une quinzaine de minutes.");
    }
    hits.push_back(now);
}

void AuthController::registerRoutes(api::Router& router) {
    using api::Access;

    // POST /api/auth/login — public
    router.post(R"(/api/auth/login)", Access::Public,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto body = http::parseBody(req);
                    const std::string identifier =
                        http::optionalString(body, "username",
                                             http::optionalString(body, "email"));
                    const std::string password = http::optionalString(body, "password");
                    http::sendJson(res, 200, auth_.login(identifier, password).toJson());
                });

    // GET /api/auth/registration — la page d'entree doit savoir si elle propose
    // ou non un formulaire d'inscription. Public, et ne revele rien d'autre.
    router.get(R"(/api/auth/registration)", Access::Public,
               [this](const httplib::Request&, httplib::Response& res) {
                   const auto mode = auth_.selfRegistrationMode();
                   http::sendJson(res, 200,
                                  {{"enabled", mode != SelfRegistration::Off},
                                   {"mode", toString(mode)},
                                   {"requires_approval", mode == SelfRegistration::Approval}});
               });

    // POST /api/auth/register — demande de compte (role Consultation impose)
    router.post(R"(/api/auth/register)", Access::Public,
                [this](const httplib::Request& req, httplib::Response& res) {
                    throttle_.check(req.remote_addr);

                    const auto body = http::parseBody(req);
                    const auto result = auth_.selfRegister(
                        http::optionalString(body, "username"),
                        http::optionalString(body, "email"),
                        http::optionalString(body, "password"),
                        http::optionalString(body, "full_name"));
                    http::sendJson(res, 201, result.toJson());
                });

    // GET /api/auth/me — profil de l'utilisateur connecte
    router.get(R"(/api/auth/me)", Access::Viewer,
               [](const httplib::Request&, httplib::Response& res) {
                   http::sendJson(res, 200, CurrentUser::require().toJson());
               });

    // POST /api/auth/password — changement de son propre mot de passe
    router.post(R"(/api/auth/password)", Access::Viewer,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto body = http::parseBody(req);
                    const auto& user = CurrentUser::require();
                    auth_.changePassword(user.id,
                                         http::optionalString(body, "current_password"),
                                         http::optionalString(body, "new_password"));
                    http::sendJson(res, 200, {{"message", "Mot de passe modifie"}});
                });

    // --------------------------------------------------- Comptes (admin)
    router.get(R"(/api/users)", Access::Admin,
               [this](const httplib::Request&, httplib::Response& res) {
                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& user : auth_.listUsers()) items.push_back(user.toJson());
                   http::sendJson(res, 200, {{"items", items}});
               });

    router.get(R"(/api/users/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   http::sendJson(res, 200, auth_.getUser(http::pathId(req)).toJson());
               });

    router.post(R"(/api/users)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto body = http::parseBody(req);
                    const auto user = auth_.createUser(
                        http::optionalString(body, "username"),
                        http::optionalString(body, "email"),
                        http::optionalString(body, "password"),
                        http::optionalString(body, "full_name"),
                        parseUserRole(http::optionalString(body, "role", "VIEWER")));
                    http::sendJson(res, 201, user.toJson());
                });

    router.put(R"(/api/users/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   const auto body = http::parseBody(req);
                   const auto user = auth_.updateUser(
                       id, http::optionalString(body, "username"),
                       http::optionalString(body, "email"),
                       http::optionalString(body, "full_name"),
                       parseUserRole(http::optionalString(body, "role", "VIEWER")),
                       http::optionalBool(body, "is_active", true));
                   http::sendJson(res, 200, user.toJson());
               });

    router.post(R"(/api/users/(\d+)/password)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto id = http::pathId(req);
                    const auto body = http::parseBody(req);
                    auth_.resetPassword(id, http::optionalString(body, "password"));
                    http::sendJson(res, 200, {{"message", "Mot de passe reinitialise"}});
                });

    router.del(R"(/api/users/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   auth_.removeUser(http::pathId(req), CurrentUser::require().id);
                   http::sendNoContent(res);
               });
}

}  // namespace app
