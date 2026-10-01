#include "controllers/ClassController.hpp"

#include "utils/Http.hpp"

namespace app {
namespace http = app::http;
namespace {

ClassRoom classFromJson(const nlohmann::json& body) {
    ClassRoom value;
    value.name = http::optionalString(body, "name");
    value.level = http::optionalString(body, "level");
    value.schoolYearId = http::nullableId(body, "school_year_id").value_or(0);
    value.mainTeacherId = http::nullableId(body, "main_teacher_id");
    value.room = http::nullableString(body, "room");
    if (body.contains("capacity") && body.at("capacity").is_number()) {
        value.capacity = body.at("capacity").get<long long>();
    }
    return value;
}

Subject subjectFromJson(const nlohmann::json& body) {
    Subject subject;
    subject.name = http::optionalString(body, "name");
    subject.code = http::optionalString(body, "code");
    subject.coefficient = http::optionalNumber(body, "coefficient", 1.0);
    subject.classId = http::nullableId(body, "class_id").value_or(0);
    subject.teacherId = http::nullableId(body, "teacher_id");
    return subject;
}

Teacher teacherFromJson(const nlohmann::json& body) {
    Teacher teacher;
    teacher.firstName = http::optionalString(body, "first_name");
    teacher.lastName = http::optionalString(body, "last_name");
    teacher.email = http::nullableString(body, "email");
    teacher.phone = http::nullableString(body, "phone");
    teacher.speciality = http::nullableString(body, "speciality");
    teacher.userId = http::nullableId(body, "user_id");
    return teacher;
}

template <typename T>
nlohmann::json toArray(const std::vector<T>& values) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& value : values) array.push_back(value.toJson());
    return array;
}

std::optional<int> termFromQuery(const httplib::Request& req) {
    if (const auto value = http::queryInt(req, "term")) return static_cast<int>(*value);
    return std::nullopt;
}

}  // namespace

void ClassController::registerRoutes(api::Router& router) {
    using api::Access;

    // ------------------------------------------------------------- Classes
    router.get(R"(/api/classes)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   ClassFilter filter;
                   filter.schoolYearId = http::queryInt(req, "school_year_id");
                   filter.level = http::queryString(req, "level");
                   filter.query = http::queryString(req, "q");
                   http::sendJson(res, 200, {{"items", toArray(classes_.list(filter))}});
               });

    router.get(R"(/api/classes/(\d+))", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   http::sendJson(res, 200, classes_.get(http::pathId(req)).toJson());
               });

    router.post(R"(/api/classes)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    http::sendJson(res, 201,
                                   classes_.create(classFromJson(http::parseBody(req))).toJson());
                });

    router.put(R"(/api/classes/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   http::sendJson(res, 200,
                                  classes_.update(id, classFromJson(http::parseBody(req))).toJson());
               });

    router.del(R"(/api/classes/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   classes_.remove(http::pathId(req));
                   http::sendNoContent(res);
               });

    // Eleves d'une classe
    router.get(R"(/api/classes/(\d+)/students)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   const bool activeOnly = http::queryBool(req, "active_only").value_or(false);
                   http::sendJson(res, 200,
                                  {{"items", toArray(students_.byClass(id, activeOnly))}});
               });

    // Matieres d'une classe
    router.get(R"(/api/classes/(\d+)/subjects)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   http::sendJson(res, 200, {{"items", toArray(classes_.subjectsOfClass(id))}});
               });

    // Classement d'une classe
    router.get(R"(/api/classes/(\d+)/ranking)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   const auto ranking = grades_.classRanking(id, termFromQuery(req));
                   nlohmann::json items = nlohmann::json::array();
                   for (const auto& entry : ranking) items.push_back(entry.toJson());
                   http::sendJson(res, 200,
                                  {{"items", items},
                                   {"class", classes_.get(id).toJson()},
                                   {"subject_statistics",
                                    grades_.classSubjectStatistics(id, termFromQuery(req))}});
               });

    // ------------------------------------------------------------ Matieres
    router.get(R"(/api/subjects)", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   if (const auto classId = http::queryInt(req, "class_id")) {
                       http::sendJson(res, 200,
                                      {{"items", toArray(classes_.subjectsOfClass(*classId))}});
                       return;
                   }
                   http::sendJson(res, 200, {{"items", toArray(classes_.allSubjects())}});
               });

    router.get(R"(/api/subjects/(\d+))", Access::Viewer,
               [this](const httplib::Request& req, httplib::Response& res) {
                   http::sendJson(res, 200, classes_.getSubject(http::pathId(req)).toJson());
               });

    router.post(R"(/api/subjects)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    http::sendJson(
                        res, 201,
                        classes_.createSubject(subjectFromJson(http::parseBody(req))).toJson());
                });

    router.put(R"(/api/subjects/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   http::sendJson(
                       res, 200,
                       classes_.updateSubject(id, subjectFromJson(http::parseBody(req))).toJson());
               });

    router.del(R"(/api/subjects/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   classes_.removeSubject(http::pathId(req));
                   http::sendNoContent(res);
               });

    // --------------------------------------------------------- Enseignants
    router.get(R"(/api/teachers)", Access::Viewer,
               [this](const httplib::Request&, httplib::Response& res) {
                   http::sendJson(res, 200, {{"items", toArray(classes_.allTeachers())}});
               });

    router.post(R"(/api/teachers)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    http::sendJson(
                        res, 201,
                        classes_.createTeacher(teacherFromJson(http::parseBody(req))).toJson());
                });

    router.put(R"(/api/teachers/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   const auto id = http::pathId(req);
                   http::sendJson(
                       res, 200,
                       classes_.updateTeacher(id, teacherFromJson(http::parseBody(req))).toJson());
               });

    router.del(R"(/api/teachers/(\d+))", Access::Admin,
               [this](const httplib::Request& req, httplib::Response& res) {
                   classes_.removeTeacher(http::pathId(req));
                   http::sendNoContent(res);
               });

    // ----------------------------------------------------- Annees scolaires
    router.get(R"(/api/school-years)", Access::Viewer,
               [this](const httplib::Request&, httplib::Response& res) {
                   http::sendJson(res, 200, {{"items", toArray(classes_.allYears())}});
               });

    router.post(R"(/api/school-years)", Access::Admin,
                [this](const httplib::Request& req, httplib::Response& res) {
                    const auto body = http::parseBody(req);
                    SchoolYear year;
                    year.label = http::optionalString(body, "label");
                    year.startDate = http::optionalString(body, "start_date");
                    year.endDate = http::optionalString(body, "end_date");
                    year.isCurrent = http::optionalBool(body, "is_current", false);
                    http::sendJson(res, 201, classes_.createYear(year).toJson());
                });
}

}  // namespace app
