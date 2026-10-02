#include "controllers/ParentController.hpp"

#include "core/Error.hpp"
#include "middleware/AuthMiddleware.hpp"
#include "utils/Http.hpp"

namespace app {
namespace http = app::http;
using middleware::CurrentUser;

void ParentController::requireOwnChild(long long studentId) {
    const auto& user = CurrentUser::require();
    // Un administrateur ou un enseignant consulte par les routes ordinaires :
    // ici, seul le lien de filiation ouvre l'acces.
    if (!users_.hasChild(user.id, studentId)) {
        throw NotFoundError("Aucun enfant rattache a votre compte sous cet identifiant");
    }
}

void ParentController::registerRoutes(api::Router& router) {
    using api::Access;

    // GET /api/parent/children — les enfants rattaches au compte connecte.
    router.get(R"(/api/parent/children)", Access::Authenticated,
               [this](const httplib::Request&, httplib::Response& res) {
                   const auto& user = CurrentUser::require();
                   nlohmann::json items = nlohmann::json::array();
                   for (const long long id : users_.childrenOf(user.id)) {
                       items.push_back(students_.get(id).toJson());
                   }
                   http::sendJson(res, 200, {{"items", items}});
               });

    // GET /api/parent/children/{id}/results — bulletin de l'enfant.
    router.get(R"(/api/parent/children/(\d+)/results)", Access::Authenticated,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   requireOwnChild(id);
                   std::optional<int> term;
                   if (const auto value = http::queryInt(req, "term")) {
                       term = static_cast<int>(*value);
                   }
                   http::sendJson(res, 200, grades_.studentResult(id, term).toJson());
               });

    // GET /api/parent/children/{id}/grades — notes de l'enfant.
    router.get(R"(/api/parent/children/(\d+)/grades)", Access::Authenticated,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   requireOwnChild(id);
                   GradeFilter filter;
                   filter.studentId = id;
                   if (const auto term = http::queryInt(req, "term")) {
                       filter.term = static_cast<int>(*term);
                   }
                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& grade : grades_.list(filter)) items.push_back(grade.toJson());
                   http::sendJson(res, 200, {{"items", items}});
               });

    // GET /api/parent/children/{id}/attendance — presences de l'enfant.
    router.get(R"(/api/parent/children/(\d+)/attendance)", Access::Authenticated,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   requireOwnChild(id);
                   AttendanceFilter filter;
                   filter.studentId = id;
                   filter.from = http::queryString(req, "from");
                   filter.to = http::queryString(req, "to");
                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& entry : attendance_.list(filter)) {
                       items.push_back(entry.toJson());
                   }
                   // Meme enveloppe que /api/students/{id}/attendance : le
                   // frontend reutilise ainsi le meme code d'affichage.
                   const auto summary = attendance_.summary(id);
                   http::sendJson(res, 200,
                                  {{"items", items},
                                   {"summary",
                                    {{"total", summary.total},
                                     {"present", summary.present},
                                     {"absent", summary.absent},
                                     {"excused", summary.excused},
                                     {"late", summary.late},
                                     {"attendance_rate", summary.attendanceRate}}}});
               });

    // ------------------------------------------- Rattachement (administration)
    router.get(R"(/api/users/(\d+)/children)", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto parentId = http::pathId(req);
                   nlohmann::json items = nlohmann::json::array();
                   for (const long long id : users_.childrenOf(parentId)) {
                       items.push_back(students_.get(id).toJson());
                   }
                   http::sendJson(res, 200, {{"items", items}});
               });

    router.post(R"(/api/users/(\d+)/children)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto parentId = http::pathId(req);
                    const auto body = http::parseBody(req);
                    if (!body.contains("student_id") || !body["student_id"].is_number()) {
                        throw ValidationError(
                            "Identifiant d'eleve obligatoire",
                            {{"fields", {{"student_id", "Ce champ est obligatoire"}}}});
                    }
                    const auto studentId = body["student_id"].get<long long>();
                    users_.linkChild(parentId, studentId,
                                     http::optionalStringOpt(body, "relation"));
                    http::sendJson(res, 201, {{"message", "Enfant rattache"}});
                });

    router.del(R"(/api/users/(\d+)/children/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto parentId = std::stoll(req.matches[1].str());
                   const auto studentId = std::stoll(req.matches[2].str());
                   if (!users_.unlinkChild(parentId, studentId)) {
                       throw NotFoundError("Ce rattachement n'existe pas");
                   }
                   http::sendNoContent(res);
               });
}

}  // namespace app
