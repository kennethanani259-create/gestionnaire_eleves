#include "controllers/ReportController.hpp"

#include "controllers/StudentController.hpp"
#include "core/Error.hpp"
#include "utils/DateTime.hpp"
#include "utils/Http.hpp"

namespace app {
namespace http = app::http;
namespace {

std::optional<int> termFromQuery(const httplib::Request& req) {
    if (const auto value = http::queryInt(req, "term")) return static_cast<int>(*value);
    return std::nullopt;
}

void sendFile(httplib::Response& res, const std::string& content, const std::string& mime,
              const std::string& filename) {
    res.status = 200;
    res.set_header("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    res.set_content(content, mime.c_str());
}

}  // namespace

void ReportController::registerRoutes(api::Router& router) {
    using api::Access;

    // Tableau de bord
    router.get(R"(/api/dashboard)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   http::sendJson(res, 200, reports_.dashboard(http::queryInt(req, "class_id")));
               });

    // Rapports disponibles
    router.get(R"(/api/reports)", Access::Viewer,
               [](const httplib::Request&, httplib::Response& res) {
                   http::sendJson(
                       res, 200,
                       {{"items",
                         {{{"code", "student_report"},
                           {"name", "Bulletin individuel (PDF)"},
                           {"url", "/api/reports/students/{id}/pdf?term="}},
                          {{"code", "class_report"},
                           {"name", "Rapport de classe (PDF)"},
                           {"url", "/api/reports/classes/{id}/pdf?term="}},
                          {{"code", "students_csv"},
                           {"name", "Export des eleves (CSV)"},
                           {"url", "/api/export/students.csv"}},
                          {{"code", "students_json"},
                           {"name", "Export des eleves (JSON)"},
                           {"url", "/api/export/students.json"}},
                          {{"code", "grades_csv"},
                           {"name", "Export des notes (CSV)"},
                           {"url", "/api/export/grades.csv"}}}}});
               });

    // Bulletin PDF d'un eleve
    router.get(R"(/api/reports/students/(\d+)/pdf)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req, 1);
                   sendFile(res, reports_.studentReportPdf(id, termFromQuery(req)),
                            "application/pdf", "bulletin-" + std::to_string(id) + ".pdf");
               });

    // Rapport PDF d'une classe
    router.get(R"(/api/reports/classes/(\d+)/pdf)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req, 1);
                   sendFile(res, reports_.classReportPdf(id, termFromQuery(req)),
                            "application/pdf", "rapport-classe-" + std::to_string(id) + ".pdf");
               });

    // Exports
    router.get(R"(/api/export/students\.csv)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto filter = StudentController::filterFromQuery(req);
                   sendFile(res, reports_.exportStudentsCsv(filter), "text/csv; charset=utf-8",
                            "eleves-" + datetime::today() + ".csv");
               });

    router.get(R"(/api/export/students\.json)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto filter = StudentController::filterFromQuery(req);
                   sendFile(res, reports_.exportStudentsJson(filter).dump(2),
                            "application/json; charset=utf-8",
                            "eleves-" + datetime::today() + ".json");
               });

    router.get(R"(/api/export/grades\.csv)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   sendFile(res,
                            reports_.exportGradesCsv(http::queryInt(req, "class_id"),
                                                     termFromQuery(req)),
                            "text/csv; charset=utf-8", "notes-" + datetime::today() + ".csv");
               });

    // Imports
    router.post(R"(/api/import/students/csv)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    std::optional<long long> classId = http::queryInt(req, "class_id");
                    std::string content = req.body;

                    // Accepte soit un corps CSV brut, soit { "content": "...", "class_id": n }
                    if (!content.empty() && content.front() == '{') {
                        const auto body = http::parseBody(req);
                        content = http::optionalString(body, "content");
                        if (const auto id = http::nullableId(body, "class_id")) classId = id;
                    }
                    if (content.empty()) throw ValidationError("Contenu CSV absent");
                    http::sendJson(res, 200,
                                   reports_.importStudentsCsv(content, classId).toJson());
                });

    router.post(R"(/api/import/students/json)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    auto payload = nlohmann::json::parse(req.body, nullptr, false);
                    if (payload.is_discarded()) throw ValidationError("JSON invalide");
                    std::optional<long long> classId = http::queryInt(req, "class_id");
                    if (payload.is_object() && payload.contains("class_id") &&
                        payload["class_id"].is_number()) {
                        classId = payload["class_id"].get<long long>();
                    }
                    http::sendJson(res, 200,
                                   reports_.importStudentsJson(payload, classId).toJson());
                });
}

}  // namespace app
