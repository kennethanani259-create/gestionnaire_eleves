#include "controllers/GradeController.hpp"

#include "core/Error.hpp"
#include "utils/Http.hpp"

namespace app {
namespace http = app::http;

Grade GradeController::gradeFromJson(const nlohmann::json& body) {
    Grade grade;
    grade.studentId = http::requireId(body, "student_id");
    grade.subjectId = http::requireId(body, "subject_id");
    grade.evalType = parseEvalType(http::optionalString(body, "eval_type", "HOMEWORK"));
    grade.score = http::requireNumber(body, "score");
    grade.maxScore = http::optionalNumber(body, "max_score", 20.0);
    grade.evalDate = http::optionalString(body, "eval_date");
    grade.term = http::optionalInt(body, "term", 1);
    grade.comment = http::nullableString(body, "comment");
    return grade;
}

Attendance GradeController::attendanceFromJson(const nlohmann::json& body) {
    Attendance entry;
    entry.studentId = http::requireId(body, "student_id");
    entry.subjectId = http::nullableId(body, "subject_id");
    entry.date = http::optionalString(body, "date");
    entry.time = http::nullableString(body, "time");
    entry.status = parseAttendanceStatus(http::optionalString(body, "status", "PRESENT"));
    entry.justification = http::nullableString(body, "justification");
    entry.comment = http::nullableString(body, "comment");
    return entry;
}

void GradeController::registerRoutes(api::Router& router) {
    using api::Access;

    // ---------------------------------------------------------------- Notes
    router.get(R"(/api/grades)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   GradeFilter filter;
                   filter.studentId = http::queryInt(req, "student_id");
                   filter.subjectId = http::queryInt(req, "subject_id");
                   filter.classId = http::queryInt(req, "class_id");
                   if (const auto term = http::queryInt(req, "term")) {
                       filter.term = static_cast<int>(*term);
                   }
                   if (const auto type = http::queryString(req, "eval_type")) {
                       filter.evalType = parseEvalType(*type);
                   }
                   filter.limit = http::queryIntOr(req, "limit", 200);
                   filter.offset = http::queryIntOr(req, "offset", 0);

                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& grade : grades_.list(filter)) items.push_back(grade.toJson());
                   http::sendJson(res, 200, {{"items", items}});
               });

    router.get(R"(/api/grades/(\d+))", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   http::sendJson(res, 200, grades_.get(http::pathId(req)).toJson());
               });

    router.post(R"(/api/grades)", Access::Teacher,
                [this](const httplib::Request& req, httplib::Response& res) {
                    http::sendJson(res, 201,
                                   grades_.create(gradeFromJson(http::parseBody(req))).toJson());
                });

    router.put(R"(/api/grades/(\d+))", Access::Teacher,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   http::sendJson(res, 200,
                                  grades_.update(id, gradeFromJson(http::parseBody(req))).toJson());
               });

    router.del(R"(/api/grades/(\d+))", Access::Teacher,
               [this](const httplib::Request& req, httplib::Response& res) {
                   grades_.remove(http::pathId(req));
                   http::sendNoContent(res);
               });

    // ----------------------------------------------------------- Presences
    router.get(R"(/api/attendance)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   AttendanceFilter filter;
                   filter.studentId = http::queryInt(req, "student_id");
                   filter.classId = http::queryInt(req, "class_id");
                   filter.from = http::queryString(req, "from");
                   filter.to = http::queryString(req, "to");
                   if (const auto status = http::queryString(req, "status")) {
                       filter.status = parseAttendanceStatus(*status);
                   }
                   filter.limit = http::queryIntOr(req, "limit", 200);
                   filter.offset = http::queryIntOr(req, "offset", 0);

                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& entry : attendance_.list(filter)) {
                       items.push_back(entry.toJson());
                   }
                   http::sendJson(res, 200, {{"items", items}});
               });

    router.post(R"(/api/attendance)", Access::Teacher,
                [this](const httplib::Request& req, httplib::Response& res) {
                    http::sendJson(
                        res, 201,
                        attendance_.create(attendanceFromJson(http::parseBody(req))).toJson());
                });

    router.put(R"(/api/attendance/(\d+))", Access::Teacher,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   http::sendJson(
                       res, 200,
                       attendance_.update(id, attendanceFromJson(http::parseBody(req))).toJson());
               });

    router.del(R"(/api/attendance/(\d+))", Access::Teacher,
               [this](const httplib::Request& req, httplib::Response& res) {
                   attendance_.remove(http::pathId(req));
                   http::sendNoContent(res);
               });

    // Appel groupe d'une classe
    router.post(R"(/api/attendance/bulk)", Access::Teacher,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto body = http::parseBody(req);
                    const long long classId = http::requireId(body, "class_id");
                    const std::string date = http::optionalString(body, "date");
                    const auto time = http::nullableString(body, "time");

                    if (!body.contains("entries") || !body.at("entries").is_array()) {
                        throw ValidationError("Le champ 'entries' doit etre un tableau");
                    }
                    std::vector<std::pair<long long, AttendanceStatus>> entries;
                    for (const auto& item : body.at("entries")) {
                        if (!item.is_object()) {
                            throw ValidationError("Chaque releve doit etre un objet JSON");
                        }
                        entries.emplace_back(
                            http::requireId(item, "student_id"),
                            parseAttendanceStatus(http::optionalString(item, "status", "PRESENT")));
                    }
                    const int created = attendance_.markClass(classId, date, entries, time);
                    http::sendJson(res, 201, {{"created", created}});
                });
}

}  // namespace app
