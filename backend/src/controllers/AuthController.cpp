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
                    // Role choisi a l'ecran : simple verification, jamais un octroi.
                    const std::string wanted = http::optionalString(body, "role");
                    std::optional<UserRole> expected;
                    if (!wanted.empty()) expected = parseUserRole(wanted);
                    http::sendJson(res, 200,
                                   auth_.login(identifier, password, expected).toJson());
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
                    const std::string username = http::optionalString(body, "username");
                    const std::string email = http::optionalString(body, "email");
                    const std::string password = http::optionalString(body, "password");
                    const std::string fullName = http::optionalString(body, "full_name");
                    const std::string schoolCode = http::optionalString(body, "school_code");
                    const std::string schoolName = http::optionalString(body, "school_name");

                    // Deux parcours : fonder un etablissement, ou en rejoindre
                    // un avec son matricule. L'un des deux est obligatoire.
                    if (!schoolName.empty()) {
                        const auto result = auth_.registerSchool(
                            schoolName, http::optionalStringOpt(body, "school_city"), username,
                            email, password, fullName);
                        http::sendJson(res, 201, result.toJson());
                        return;
                    }
                    if (schoolCode.empty()) {
                        throw ValidationError(
                            "Indiquez le matricule de votre etablissement, ou le nom de "
                            "l'ecole que vous creez",
                            {{"fields", {{"school_code", "Ce champ est obligatoire"}}}});
                    }
                    const auto role = parseUserRole(
                        http::optionalString(body, "role", "VIEWER"));
                    const auto result =
                        auth_.joinSchool(schoolCode, role, username, email, password, fullName);
                    http::sendJson(res, 201, result.toJson());
                });

    // GET /api/auth/schools/{code} — confirme l'ecole saisie a l'inscription.
    // Publique mais volontairement avare : nom et ville, rien d'autre.
    router.get(R"(/api/auth/schools/([A-Za-z0-9]{4,16}))", Access::Public,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto school = auth_.schoolByCode(req.matches[1].str());
                   if (!school.has_value()) {
                       throw NotFoundError("Aucun etablissement ne porte ce matricule");
                   }
                   http::sendJson(res, 200, school->toPublicJson());
               });

    // GET /api/school — etablissement de l'utilisateur connecte.
    router.get(R"(/api/school)", Access::Authenticated,
               [this](const httplib::Request&, httplib::Response& res) {
                   const auto school = auth_.schoolOf(CurrentUser::require());
                   if (!school.has_value()) throw NotFoundError("Aucun etablissement rattache");
                   // Le matricule n'est montre qu'a l'administration : c'est la
                   // cle d'entree dans l'ecole.
                   const bool isAdmin = CurrentUser::require().role == UserRole::Admin;
                   http::sendJson(res, 200, isAdmin ? school->toJson() : school->toPublicJson());
               });

    // POST /api/school/code — regenere le matricule (administration).
    router.post(R"(/api/school/code)", Access::Admin,
                [this](const httplib::Request&, httplib::Response& res) {
                    const auto school = auth_.regenerateSchoolCode(CurrentUser::require());
                    http::sendJson(res, 200, school.toJson());
                });

    // GET /api/auth/me — profil de l'utilisateur connecte
    router.get(R"(/api/auth/me)", Access::Authenticated,
               [](const httplib::Request&, httplib::Response& res) {
                   http::sendJson(res, 200, CurrentUser::require().toJson());
               });

    // POST /api/auth/password — changement de son propre mot de passe
    router.post(R"(/api/auth/password)", Access::Authenticated,
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
