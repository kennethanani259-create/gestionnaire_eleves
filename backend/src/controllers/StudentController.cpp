#include "controllers/StudentController.hpp"

#include "core/Error.hpp"
#include "utils/Http.hpp"

namespace app {
namespace http = app::http;

Student StudentController::fromJson(const nlohmann::json& body) {
    Student student;
    student.matricule = http::optionalString(body, "matricule");
    student.firstName = http::optionalString(body, "first_name");
    student.lastName = http::optionalString(body, "last_name");
    student.birthDate = http::optionalString(body, "birth_date");
    student.gender = parseGender(http::optionalString(body, "gender", "M"));
    student.address = http::nullableString(body, "address");
    student.phone = http::nullableString(body, "phone");
    student.email = http::nullableString(body, "email");
    student.guardianName = http::nullableString(body, "guardian_name");
    student.guardianPhone = http::nullableString(body, "guardian_phone");
    student.enrollmentDate = http::optionalString(body, "enrollment_date");
    student.status = parseStudentStatus(http::optionalString(body, "status", "ACTIVE"));
    student.photoPath = http::nullableString(body, "photo_path");
    student.classId = http::nullableId(body, "class_id");
    return student;
}

StudentFilter StudentController::filterFromQuery(const httplib::Request& req) {
    StudentFilter filter;
    filter.query = http::queryString(req, "q");
    filter.classId = http::queryInt(req, "class_id");
    filter.level = http::queryString(req, "level");
    if (const auto status = http::queryString(req, "status")) {
        filter.status = parseStudentStatus(*status);
    }
    if (const auto gender = http::queryString(req, "gender")) {
        filter.gender = parseGender(*gender);
    }
    filter.sortBy = http::queryString(req, "sort_by").value_or("last_name");
    filter.descending = http::queryString(req, "sort_dir").value_or("asc") == "desc";
    filter.limit = http::queryIntOr(req, "limit", 50);
    filter.offset = http::queryIntOr(req, "offset", 0);
    return filter;
}

void StudentController::registerRoutes(api::Router& router) {
    using api::Access;

    // GET /api/students — liste paginee, filtrable et triable
    router.get(R"(/api/students)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto page = students_.list(filterFromQuery(req));
                   http::sendJson(res, 200, page.toJson());
               });

    // GET /api/students/{id}
    router.get(R"(/api/students/(\d+))", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto student = students_.get(http::pathId(req));
                   http::sendJson(res, 200, student.toJson());
               });

    // POST /api/students
    router.post(R"(/api/students)", Access::Teacher,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto created = students_.create(fromJson(http::parseBody(req)));
                    http::sendJson(res, 201, created.toJson());
                });

    // PUT /api/students/{id}
    router.put(R"(/api/students/(\d+))", Access::Teacher,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   const auto updated = students_.update(id, fromJson(http::parseBody(req)));
                   http::sendJson(res, 200, updated.toJson());
               });

    // DELETE /api/students/{id}
    router.del(R"(/api/students/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   students_.remove(http::pathId(req));
                   http::sendNoContent(res);
               });

    // GET /api/students/{id}/grades
    router.get(R"(/api/students/(\d+)/grades)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   students_.get(id);  // 404 si l'eleve n'existe pas
                   GradeFilter filter;
                   filter.studentId = id;
                   if (const auto term = http::queryInt(req, "term")) {
                       filter.term = static_cast<int>(*term);
                   }
                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& grade : grades_.list(filter)) items.push_back(grade.toJson());
                   http::sendJson(res, 200, {{"items", items}});
               });

    // POST /api/students/{id}/grades
    router.post(R"(/api/students/(\d+)/grades)", Access::Teacher,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto studentId = http::pathId(req);
                    const auto body = http::parseBody(req);
                    Grade grade;
                    grade.studentId = studentId;
                    grade.subjectId = http::requireId(body, "subject_id");
                    grade.evalType = parseEvalType(http::optionalString(body, "eval_type", "HOMEWORK"));
                    grade.score = http::requireNumber(body, "score");
                    grade.maxScore = http::optionalNumber(body, "max_score", 20.0);
                    grade.evalDate = http::optionalString(body, "eval_date");
                    grade.term = http::optionalInt(body, "term", 1);
                    grade.comment = http::nullableString(body, "comment");
                    http::sendJson(res, 201, grades_.create(grade).toJson());
                });

    // GET /api/students/{id}/attendance
    router.get(R"(/api/students/(\d+)/attendance)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   students_.get(id);
                   AttendanceFilter filter;
                   filter.studentId = id;
                   filter.from = http::queryString(req, "from");
                   filter.to = http::queryString(req, "to");
                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& entry : attendance_.list(filter)) {
                       items.push_back(entry.toJson());
                   }
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

    // POST /api/students/{id}/attendance
    router.post(R"(/api/students/(\d+)/attendance)", Access::Teacher,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto studentId = http::pathId(req);
                    const auto body = http::parseBody(req);
                    Attendance entry;
                    entry.studentId = studentId;
                    entry.subjectId = http::nullableId(body, "subject_id");
                    entry.date = http::optionalString(body, "date");
                    entry.time = http::nullableString(body, "time");
                    entry.status = parseAttendanceStatus(
                        http::optionalString(body, "status", "PRESENT"));
                    entry.justification = http::nullableString(body, "justification");
                    entry.comment = http::nullableString(body, "comment");
                    http::sendJson(res, 201, attendance_.create(entry).toJson());
                });

    // GET /api/students/{id}/results — bilan complet (moyennes, rang, presences)
    router.get(R"(/api/students/(\d+)/results)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   std::optional<int> term;
                   if (const auto value = http::queryInt(req, "term")) {
                       term = static_cast<int>(*value);
                   }
                   http::sendJson(res, 200, grades_.studentResult(id, term).toJson());
               });
}

}  // namespace app
